#include "cmd_request_service.h"
#include <boost/exception/all.hpp>
#include <boost/xpressive/xpressive_dynamic.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>
#include "server/server.h"
#include "config/conf_manager.h"
#include "service_module/service_message_id.h"
#include "../message/protocol_coder/matrix_coder.h"
#include "../message/message_id.h"
#include "log/log.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"
#include "rapidjson/error/error.h"

int32_t cmd_request_service::service_init(bpo::variables_map &options) {
    return E_SUCCESS;
}

void cmd_request_service::init_subscription() {
    // create task
    SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_create_task_req).name())
    SUBSCRIBE_BUS_MESSAGE(NODE_CREATE_TASK_RSP)
    // start task
    SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_start_task_req).name())
    SUBSCRIBE_BUS_MESSAGE(NODE_START_TASK_RSP)
    // stop task
    SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_stop_task_req).name())
    SUBSCRIBE_BUS_MESSAGE(NODE_STOP_TASK_RSP)
    // restart task
    SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_restart_task_req).name())
    SUBSCRIBE_BUS_MESSAGE(NODE_RESTART_TASK_RSP)
    // reset task
    SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_reset_task_req).name())
    SUBSCRIBE_BUS_MESSAGE(NODE_RESET_TASK_RSP)
    // delete task
    SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_delete_task_req).name())
    SUBSCRIBE_BUS_MESSAGE(NODE_DELETE_TASK_RSP)
    // task logs
    SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_task_logs_req).name())
    SUBSCRIBE_BUS_MESSAGE(NODE_TASK_LOGS_RSP)
    // list task
    SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_list_task_req).name())
    SUBSCRIBE_BUS_MESSAGE(NODE_LIST_TASK_RSP)
    // modify task
    SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_modify_task_req).name())
    SUBSCRIBE_BUS_MESSAGE(NODE_MODIFY_TASK_RSP)
    // list node
    SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_list_node_req).name())
    SUBSCRIBE_BUS_MESSAGE(NODE_QUERY_NODE_INFO_RSP)

    //forward binary message
    SUBSCRIBE_BUS_MESSAGE(BINARY_FORWARD_MSG);
}

void cmd_request_service::init_invoker() {
    invoker_type invoker;
    // create task
    BIND_MESSAGE_INVOKER(typeid(::cmd_create_task_req).name(), &cmd_request_service::on_cmd_create_task_req)
    BIND_MESSAGE_INVOKER(NODE_CREATE_TASK_RSP, &cmd_request_service::on_node_create_task_rsp)
    // start task
    BIND_MESSAGE_INVOKER(typeid(::cmd_start_task_req).name(), &cmd_request_service::on_cmd_start_task_req)
    BIND_MESSAGE_INVOKER(NODE_START_TASK_RSP, &cmd_request_service::on_node_start_task_rsp)
    // stop task
    BIND_MESSAGE_INVOKER(typeid(::cmd_stop_task_req).name(), &cmd_request_service::on_cmd_stop_task_req)
    BIND_MESSAGE_INVOKER(NODE_STOP_TASK_RSP, &cmd_request_service::on_node_stop_task_rsp)
    // restart task
    BIND_MESSAGE_INVOKER(typeid(::cmd_restart_task_req).name(), &cmd_request_service::on_cmd_restart_task_req)
    BIND_MESSAGE_INVOKER(NODE_RESTART_TASK_RSP, &cmd_request_service::on_node_restart_task_rsp)
    // reset task
    BIND_MESSAGE_INVOKER(typeid(::cmd_reset_task_req).name(), &cmd_request_service::on_cmd_reset_task_req)
    BIND_MESSAGE_INVOKER(NODE_RESET_TASK_RSP, &cmd_request_service::on_node_reset_task_rsp)
    // delete task
    BIND_MESSAGE_INVOKER(typeid(::cmd_delete_task_req).name(), &cmd_request_service::on_cmd_delete_task_req)
    BIND_MESSAGE_INVOKER(NODE_DELETE_TASK_RSP, &cmd_request_service::on_node_delete_task_rsp)
    // task logs
    BIND_MESSAGE_INVOKER(typeid(::cmd_task_logs_req).name(), &cmd_request_service::on_cmd_task_logs_req);
    BIND_MESSAGE_INVOKER(NODE_TASK_LOGS_RSP, &cmd_request_service::on_node_task_logs_rsp);
    // list task
    BIND_MESSAGE_INVOKER(typeid(::cmd_list_task_req).name(), &cmd_request_service::on_cmd_list_task_req);
    BIND_MESSAGE_INVOKER(NODE_LIST_TASK_RSP, &cmd_request_service::on_node_list_task_rsp);
    // modify task
    BIND_MESSAGE_INVOKER(typeid(::cmd_modify_task_req).name(), &cmd_request_service::on_cmd_modify_task_req);
    BIND_MESSAGE_INVOKER(NODE_MODIFY_TASK_RSP, &cmd_request_service::on_node_modify_task_rsp);
    // list node
    BIND_MESSAGE_INVOKER(typeid(::cmd_list_node_req).name(), &cmd_request_service::on_cmd_query_node_info_req)
    BIND_MESSAGE_INVOKER(NODE_QUERY_NODE_INFO_RSP, &cmd_request_service::on_node_query_node_info_rsp)

    BIND_MESSAGE_INVOKER(BINARY_FORWARD_MSG, &cmd_request_service::on_binary_forward);
}

void cmd_request_service::init_timer() {
    // create task
    m_timer_invokers[NODE_CREATE_TASK_TIMER] = std::bind(&cmd_request_service::on_node_create_task_timer, this, std::placeholders::_1);
    // start task
    m_timer_invokers[NODE_START_TASK_TIMER] = std::bind(&cmd_request_service::on_node_start_task_timer, this, std::placeholders::_1);
    // stop task
    m_timer_invokers[NODE_STOP_TASK_TIMER] = std::bind(&cmd_request_service::on_node_stop_task_timer, this, std::placeholders::_1);
    // restart task
    m_timer_invokers[NODE_RESTART_TASK_TIMER] = std::bind(&cmd_request_service::on_node_restart_task_timer, this, std::placeholders::_1);
    // reset task
    m_timer_invokers[NODE_RESET_TASK_TIMER] = std::bind(&cmd_request_service::on_node_reset_task_timer, this, std::placeholders::_1);
    // destroy task
    m_timer_invokers[NODE_DELETE_TASK_TIMER] = std::bind(&cmd_request_service::on_node_delete_task_timer, this, std::placeholders::_1);
    // task logs
    m_timer_invokers[NODE_TASK_LOGS_TIMER] = std::bind(&cmd_request_service::on_node_task_logs_timer, this, std::placeholders::_1);
    // list task
    m_timer_invokers[NODE_LIST_TASK_TIMER] = std::bind(&cmd_request_service::on_node_list_task_timer, this, std::placeholders::_1);
    // modify task
    m_timer_invokers[NODE_MODIFY_TASK_TIMER] = std::bind(&cmd_request_service::on_node_modify_task_timer, this, std::placeholders::_1);
    // list node
    m_timer_invokers[NODE_QUERY_NODE_INFO_TIMER] = std::bind(&cmd_request_service::on_node_query_node_info_timer, this, std::placeholders::_1);
}

// create task
std::shared_ptr<dbc::network::message> cmd_request_service::create_node_create_task_req_msg(const std::shared_ptr<::cmd_create_task_req> &cmd_req) {
    if (cmd_req->peer_nodes_list.empty()) {
        LOG_ERROR << "create failed: no peer_nodes_list!";
        return nullptr;
    }

    auto req_content = std::make_shared<dbc::node_create_task_req>();
    // header
    req_content->header.__set_magic(conf_manager::instance().get_net_flag());
    req_content->header.__set_msg_name(NODE_CREATE_TASK_REQ);
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

    std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
    req_msg->set_name(NODE_CREATE_TASK_REQ);
    req_msg->set_content(req_content);

    // 创建定时器
    uint32_t timer_id = this->add_timer(NODE_CREATE_TASK_TIMER, conf_manager::instance().get_timer_dbc_request_in_millisecond(),
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

int32_t cmd_request_service::on_cmd_create_task_req(std::shared_ptr<dbc::network::message> &msg) {
    std::shared_ptr<::cmd_create_task_req> cmd_req_msg = std::dynamic_pointer_cast<::cmd_create_task_req>(msg->get_content());
    if (cmd_req_msg == nullptr) {
        LOG_ERROR << "cmd_req_msg is null";
        return E_DEFAULT;
    }

    auto node_req_msg = create_node_create_task_req_msg(cmd_req_msg);
    if (nullptr == node_req_msg) {
        std::shared_ptr<::cmd_create_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_create_task_rsp>();
        cmd_rsp_msg->result = E_DEFAULT;
        cmd_rsp_msg->result_info = "creaate node request failed";
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);

        topic_manager::instance().publish<void>(typeid(::cmd_create_task_rsp).name(), cmd_rsp_msg);
        return E_DEFAULT;
    }

    if (dbc::network::connection_manager::instance().broadcast_message(node_req_msg) != E_SUCCESS) {
        std::shared_ptr<::cmd_create_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_create_task_rsp>();
        cmd_rsp_msg->result = E_DEFAULT;
        cmd_rsp_msg->result_info = "broadcast failed";
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);

        topic_manager::instance().publish<void>(typeid(::cmd_create_task_rsp).name(), cmd_rsp_msg);
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

int32_t cmd_request_service::on_node_create_task_rsp(std::shared_ptr<dbc::network::message> &msg) {
    if (!check_rsp_header(msg)) {
        LOG_ERROR << "rsp header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_create_task_rsp> rsp_content =
            std::dynamic_pointer_cast<dbc::node_create_task_rsp>(msg->content);
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

    std::shared_ptr<::cmd_create_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_create_task_rsp>();
    cmd_rsp_msg->header.__set_session_id(rsp_content->header.session_id);
    cmd_rsp_msg->result = rsp_result;
    cmd_rsp_msg->result_info = rsp_result_msg;

    topic_manager::instance().publish<void>(typeid(::cmd_create_task_rsp).name(), cmd_rsp_msg);

    this->remove_timer(session->get_timer_id());
    session->clear();
    this->remove_session(session->get_session_id());
    return E_SUCCESS;
}

int32_t cmd_request_service::on_node_create_task_timer(const std::shared_ptr<core_timer> &timer) {
    std::shared_ptr<::cmd_create_task_rsp> cmd_rsp = std::make_shared<::cmd_create_task_rsp>();
    if (nullptr == timer) {
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "timer is nullptr";
        topic_manager::instance().publish<void>(typeid(::cmd_create_task_rsp).name(), cmd_rsp);
        return E_DEFAULT;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "session is nullptr";
        topic_manager::instance().publish<void>(typeid(::cmd_create_task_rsp).name(), cmd_rsp);
        return E_DEFAULT;
    }

    cmd_rsp->header.__set_session_id(session_id);
    cmd_rsp->result = E_DEFAULT;
    std::stringstream ss;
    ss << "{";
    ss << "\"error_code\":" << E_DEFAULT;
    ss << ", \"error_message\":\"create task timeout\"";
    ss << "}";
    cmd_rsp->result_info = ss.str();

    topic_manager::instance().publish<void>(typeid(::cmd_create_task_rsp).name(), cmd_rsp);

    session->clear();
    this->remove_session(session_id);
    return E_SUCCESS;
}

// start task
std::shared_ptr<dbc::network::message> cmd_request_service::create_node_start_task_req_msg(
        const std::shared_ptr<::cmd_start_task_req> &cmd_req) {
    if (cmd_req->task_id.empty()) {
        LOG_ERROR << "start failed: task_id is empty!";
        return nullptr;
    }

    if (cmd_req->peer_nodes_list.empty()) {
        LOG_ERROR << "start failed: no peer_nodes_list!";
        return nullptr;
    }

    // 创建 node_ 请求
    auto req_content = std::make_shared<dbc::node_start_task_req>();
    // header
    req_content->header.__set_magic(conf_manager::instance().get_net_flag());
    req_content->header.__set_msg_name(NODE_START_TASK_REQ);
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
    req_msg->set_name(NODE_START_TASK_REQ);
    req_msg->set_content(req_content);

    // 创建定时器
    uint32_t timer_id = this->add_timer(NODE_START_TASK_TIMER, conf_manager::instance().get_timer_dbc_request_in_millisecond(),
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

int32_t cmd_request_service::on_cmd_start_task_req(std::shared_ptr<dbc::network::message> &msg) {
    std::shared_ptr<::cmd_start_task_req> cmd_req_msg = std::dynamic_pointer_cast<::cmd_start_task_req>(msg->get_content());
    if (cmd_req_msg == nullptr) {
        LOG_ERROR << "cmd_req_msg is null";
        return E_NULL_POINTER;
    }

    auto node_req_msg = create_node_start_task_req_msg(cmd_req_msg);
    if (nullptr == node_req_msg) {
        std::shared_ptr<::cmd_start_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_start_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
        cmd_rsp_msg->result = E_DEFAULT;
        cmd_rsp_msg->result_info = "create node request failed!";

        topic_manager::instance().publish<void>(typeid(::cmd_start_task_rsp).name(), cmd_rsp_msg);
        return E_DEFAULT;
    }

    if (dbc::network::connection_manager::instance().broadcast_message(node_req_msg) != E_SUCCESS) {
        std::shared_ptr<::cmd_start_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_start_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
        cmd_rsp_msg->result = E_DEFAULT;
        cmd_rsp_msg->result_info = "submit error. pls check network";

        topic_manager::instance().publish<void>(typeid(::cmd_start_task_rsp).name(), cmd_rsp_msg);
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

int32_t cmd_request_service::on_node_start_task_rsp(std::shared_ptr<dbc::network::message> &msg) {
    if (!check_rsp_header(msg)) {
        LOG_ERROR << "rsp header check failed";
        return E_DEFAULT;
    }

    std::shared_ptr<dbc::node_start_task_rsp> rsp_content = std::dynamic_pointer_cast<dbc::node_start_task_rsp>(msg->content);
    if (!rsp_content) {
        LOG_ERROR << "rsp is nullptr";
        return E_DEFAULT;
    }

    if (!check_nonce(rsp_content->header.nonce)) {
        LOG_ERROR << "nonce check failed";
        return E_DEFAULT;
    }

    // check rsp body
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

    std::shared_ptr<::cmd_start_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_start_task_rsp>();
    cmd_rsp_msg->header.__set_session_id(rsp_content->header.session_id);
    cmd_rsp_msg->result = rsp_result;
    cmd_rsp_msg->result_info = rsp_result_msg;

    topic_manager::instance().publish<void>(typeid(::cmd_start_task_rsp).name(), cmd_rsp_msg);

    this->remove_timer(session->get_timer_id());
    session->clear();
    this->remove_session(session->get_session_id());
    return E_SUCCESS;
}

int32_t cmd_request_service::on_node_start_task_timer(const std::shared_ptr<core_timer> &timer) {
    std::shared_ptr<::cmd_start_task_rsp> cmd_rsp = std::make_shared<::cmd_start_task_rsp>();
    if (nullptr == timer) {
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "timer is nullptr";
        topic_manager::instance().publish<void>(typeid(::cmd_start_task_rsp).name(), cmd_rsp);
        return E_DEFAULT;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "session is nullptr";
        topic_manager::instance().publish<void>(typeid(::cmd_start_task_rsp).name(), cmd_rsp);
        return E_DEFAULT;
    }

    cmd_rsp->header.__set_session_id(session_id);
    cmd_rsp->result = E_DEFAULT;
    std::stringstream ss;
    ss << "{";
    ss << "\"error_code\":" << E_DEFAULT;
    ss << ", \"error_message\":\"start task timeout\"";
    ss << "}";
    cmd_rsp->result_info = ss.str();

    topic_manager::instance().publish<void>(typeid(::cmd_start_task_rsp).name(), cmd_rsp);

    session->clear();
    this->remove_session(session_id);
    return E_SUCCESS;
}

// stop task
std::shared_ptr<dbc::network::message> cmd_request_service::create_node_stop_task_req_msg(const std::shared_ptr<::cmd_stop_task_req> &cmd_req) {
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

int32_t cmd_request_service::on_cmd_stop_task_req(const std::shared_ptr<dbc::network::message> &msg) {
    std::shared_ptr<::cmd_stop_task_req> cmd_req_msg = std::dynamic_pointer_cast<::cmd_stop_task_req>(
            msg->get_content());
    if (cmd_req_msg == nullptr) {
        LOG_ERROR << "cmd_req_msg is null";
        return E_NULL_POINTER;
    }

    auto task_req_msg = create_node_stop_task_req_msg(cmd_req_msg);
    if (nullptr == task_req_msg) {
        std::shared_ptr<::cmd_stop_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_stop_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
        cmd_rsp_msg->result = E_DEFAULT;
        cmd_rsp_msg->result_info = "create node request failed!";

        topic_manager::instance().publish<void>(typeid(::cmd_stop_task_rsp).name(), cmd_rsp_msg);
        return E_DEFAULT;
    }

    if (E_SUCCESS != dbc::network::connection_manager::instance().broadcast_message(task_req_msg)) {
        std::shared_ptr<::cmd_stop_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_stop_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
        cmd_rsp_msg->result = E_INACTIVE_CHANNEL;
        cmd_rsp_msg->result_info = "dbc node don't connect to network, pls check ";

        topic_manager::instance().publish<void>(typeid(::cmd_stop_task_rsp).name(), cmd_rsp_msg);
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

int32_t cmd_request_service::on_node_stop_task_rsp(std::shared_ptr<dbc::network::message> &msg) {
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

int32_t cmd_request_service::on_node_stop_task_timer(const std::shared_ptr<core_timer> &timer) {
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
std::shared_ptr<dbc::network::message> cmd_request_service::create_node_restart_task_req_msg(
        const std::shared_ptr<::cmd_restart_task_req> &cmd_req) {
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

int32_t cmd_request_service::on_cmd_restart_task_req(std::shared_ptr<dbc::network::message> &msg) {
    std::shared_ptr<::cmd_restart_task_req> cmd_req_msg = std::dynamic_pointer_cast<::cmd_restart_task_req>(
            msg->get_content());
    if (cmd_req_msg == nullptr) {
        LOG_ERROR << "cmd_req_msg is null";
        return E_NULL_POINTER;
    }

    auto task_req_msg = create_node_restart_task_req_msg(cmd_req_msg);
    if (nullptr == task_req_msg) {
        std::shared_ptr<::cmd_restart_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_restart_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
        cmd_rsp_msg->result = E_DEFAULT;
        cmd_rsp_msg->result_info = "create node request failed!";

        topic_manager::instance().publish<void>(typeid(::cmd_restart_task_rsp).name(), cmd_rsp_msg);
        return E_DEFAULT;
    }

    if (dbc::network::connection_manager::instance().broadcast_message(task_req_msg) != E_SUCCESS) {
        std::shared_ptr<::cmd_restart_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_restart_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
        cmd_rsp_msg->result = E_DEFAULT;
        cmd_rsp_msg->result_info = "submit error. pls check network";

        topic_manager::instance().publish<void>(typeid(::cmd_restart_task_rsp).name(), cmd_rsp_msg);
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

int32_t cmd_request_service::on_node_restart_task_rsp(std::shared_ptr<dbc::network::message> &msg) {
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

int32_t cmd_request_service::on_node_restart_task_timer(const std::shared_ptr<core_timer> &timer) {
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
std::shared_ptr<dbc::network::message> cmd_request_service::create_node_reset_task_req_msg(const std::shared_ptr<::cmd_reset_task_req> &cmd_req) {
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

int32_t cmd_request_service::on_cmd_reset_task_req(const std::shared_ptr<dbc::network::message> &msg) {
    std::shared_ptr<::cmd_reset_task_req> cmd_req_msg = std::dynamic_pointer_cast<::cmd_reset_task_req>(
            msg->get_content());
    if (cmd_req_msg == nullptr) {
        LOG_ERROR << "cmd_req_msg is null";
        return E_NULL_POINTER;
    }

    auto task_req_msg = create_node_reset_task_req_msg(cmd_req_msg);
    if (nullptr == task_req_msg) {
        std::shared_ptr<::cmd_reset_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_reset_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
        cmd_rsp_msg->result = E_DEFAULT;
        cmd_rsp_msg->result_info = "create node request failed!";

        topic_manager::instance().publish<void>(typeid(::cmd_reset_task_rsp).name(), cmd_rsp_msg);
        return E_DEFAULT;
    }

    if (E_SUCCESS != dbc::network::connection_manager::instance().broadcast_message(task_req_msg)) {
        std::shared_ptr<::cmd_reset_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_reset_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
        cmd_rsp_msg->result = E_INACTIVE_CHANNEL;
        cmd_rsp_msg->result_info = "dbc node don't connect to network, pls check ";

        topic_manager::instance().publish<void>(typeid(::cmd_reset_task_rsp).name(), cmd_rsp_msg);
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

int32_t cmd_request_service::on_node_reset_task_rsp(std::shared_ptr<dbc::network::message> &msg) {
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

int32_t cmd_request_service::on_node_reset_task_timer(const std::shared_ptr<core_timer> &timer) {
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
std::shared_ptr<dbc::network::message> cmd_request_service::create_node_delete_task_req_msg(const std::shared_ptr<::cmd_delete_task_req> &cmd_req) {
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

int32_t cmd_request_service::on_cmd_delete_task_req(const std::shared_ptr<dbc::network::message> &msg) {
    std::shared_ptr<::cmd_delete_task_req> cmd_req_msg = std::dynamic_pointer_cast<::cmd_delete_task_req>(
            msg->get_content());
    if (cmd_req_msg == nullptr) {
        LOG_ERROR << "cmd_req_msg is null";
        return E_NULL_POINTER;
    }

    auto task_req_msg = create_node_delete_task_req_msg(cmd_req_msg);
    if (nullptr == task_req_msg) {
        std::shared_ptr<::cmd_delete_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_delete_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
        cmd_rsp_msg->result = E_DEFAULT;
        cmd_rsp_msg->result_info = "create node request failed!";

        topic_manager::instance().publish<void>(typeid(::cmd_delete_task_rsp).name(), cmd_rsp_msg);
        return E_DEFAULT;
    }

    if (E_SUCCESS != dbc::network::connection_manager::instance().broadcast_message(task_req_msg)) {
        std::shared_ptr<::cmd_delete_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_delete_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
        cmd_rsp_msg->result = E_DEFAULT;
        cmd_rsp_msg->result_info = "dbc node don't connect to network, pls check ";

        topic_manager::instance().publish<void>(typeid(::cmd_delete_task_rsp).name(), cmd_rsp_msg);
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

int32_t cmd_request_service::on_node_delete_task_rsp(std::shared_ptr<dbc::network::message> &msg) {
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

int32_t cmd_request_service::on_node_delete_task_timer(const std::shared_ptr<core_timer> &timer) {
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

// task logs
std::shared_ptr<dbc::network::message>
cmd_request_service::create_node_task_logs_req_msg(const std::shared_ptr<::cmd_task_logs_req> &cmd_req) {
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

int32_t cmd_request_service::on_cmd_task_logs_req(const std::shared_ptr<dbc::network::message> &msg) {
    auto cmd_req_msg = std::dynamic_pointer_cast<::cmd_task_logs_req>(msg->get_content());
    if (nullptr == cmd_req_msg) {
        LOG_ERROR << "ai power requester service cmd logs msg content nullptr";
        return E_DEFAULT;
    }

    // 创建 node_ 请求
    auto task_req_msg = create_node_task_logs_req_msg(cmd_req_msg);
    if (nullptr == task_req_msg) {
        std::shared_ptr<::cmd_task_logs_rsp> cmd_rsp_msg = std::make_shared<::cmd_task_logs_rsp>();
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
        cmd_rsp_msg->result = E_DEFAULT;
        cmd_rsp_msg->result_info = "create node request failed!";

        topic_manager::instance().publish<void>(typeid(::cmd_task_logs_rsp).name(), cmd_rsp_msg);
        return E_DEFAULT;
    }

    if (E_SUCCESS != dbc::network::connection_manager::instance().broadcast_message(task_req_msg)) {
        std::shared_ptr<::cmd_task_logs_rsp> cmd_rsp_msg = std::make_shared<::cmd_task_logs_rsp>();
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
        cmd_rsp_msg->result = E_DEFAULT;
        cmd_rsp_msg->result_info = "dbc node don't connect to network, pls check ";

        topic_manager::instance().publish<void>(typeid(::cmd_task_logs_rsp).name(), cmd_rsp_msg);
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

int32_t cmd_request_service::on_node_task_logs_rsp(std::shared_ptr<dbc::network::message> &msg) {
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

int32_t cmd_request_service::on_node_task_logs_timer(std::shared_ptr<core_timer> timer) {
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

// list tasks
std::shared_ptr<dbc::network::message> cmd_request_service::create_node_list_task_req_msg(
        const std::shared_ptr<::cmd_list_task_req> &cmd_req) {
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

int32_t cmd_request_service::on_cmd_list_task_req(const std::shared_ptr<dbc::network::message> &msg) {
    auto cmd_req_msg = std::dynamic_pointer_cast<::cmd_list_task_req>(msg->get_content());
    if (nullptr == cmd_req_msg) {
        LOG_ERROR << "ai power requester service cmd logs msg content nullptr";
        return E_DEFAULT;
    }

    // 创建 node_ 请求
    auto task_req_msg = create_node_list_task_req_msg(cmd_req_msg);
    if (nullptr == task_req_msg) {
        auto cmd_rsp_msg = std::make_shared<::cmd_list_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
        cmd_rsp_msg->result = E_DEFAULT;
        cmd_rsp_msg->result_info = "create node request failed!";

        topic_manager::instance().publish<void>(typeid(::cmd_list_task_rsp).name(), cmd_rsp_msg);
        return E_DEFAULT;
    }

    if (E_SUCCESS != dbc::network::connection_manager::instance().broadcast_message(task_req_msg)) {
        auto cmd_rsp_msg = std::make_shared<::cmd_list_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
        cmd_rsp_msg->result = E_DEFAULT;
        cmd_rsp_msg->result_info = "dbc node don't connect to network, pls check ";

        topic_manager::instance().publish<void>(typeid(::cmd_list_task_rsp).name(), cmd_rsp_msg);
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

int32_t cmd_request_service::on_node_list_task_rsp(std::shared_ptr<dbc::network::message> &msg) {
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

int32_t cmd_request_service::on_node_list_task_timer(const std::shared_ptr<core_timer> &timer) {
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
std::shared_ptr<dbc::network::message> cmd_request_service::create_node_modify_task_req_msg(
        const std::shared_ptr<::cmd_modify_task_req> &cmd_req) {

    return nullptr;
}

int32_t cmd_request_service::on_cmd_modify_task_req(const std::shared_ptr<dbc::network::message> &msg) {

    return E_SUCCESS;
}

int32_t cmd_request_service::on_node_modify_task_rsp(std::shared_ptr<dbc::network::message> &msg) {

    return E_SUCCESS;
}

int32_t cmd_request_service::on_node_modify_task_timer(const std::shared_ptr<core_timer> &timer) {

    return E_SUCCESS;
}

// list node
std::shared_ptr<dbc::network::message> cmd_request_service::create_node_query_node_info_req_msg(const std::shared_ptr<::cmd_list_node_req> &cmd_req) {
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

int32_t cmd_request_service::on_cmd_query_node_info_req(std::shared_ptr<dbc::network::message> &msg)
{
    std::shared_ptr<::cmd_list_node_req> cmd_req_msg = std::dynamic_pointer_cast<::cmd_list_node_req>(msg->get_content());
    if (cmd_req_msg == nullptr) {
        LOG_ERROR << "cmd_req_msg is null";
        return E_DEFAULT;
    }

    if (cmd_req_msg->peer_nodes_list.empty()) {
        auto cmd_resp = std::make_shared<::cmd_list_node_rsp>();
        cmd_resp->header.__set_session_id(cmd_req_msg->header.session_id);
        cmd_resp->id_2_services = service_info_collection::instance().get_all();

        topic_manager::instance().publish<void>(typeid(::cmd_list_node_rsp).name(), cmd_resp);
        return E_SUCCESS;
    }
    else {
        auto node_req_msg = create_node_query_node_info_req_msg(cmd_req_msg);
        if(node_req_msg == nullptr) {
            auto cmd_resp = std::make_shared<::cmd_list_node_rsp>();
            cmd_resp->result = E_DEFAULT;
            cmd_resp->result_info = "create node request failed";
            cmd_resp->header.__set_session_id(cmd_req_msg->header.session_id);

            topic_manager::instance().publish<void>(typeid(::cmd_list_node_rsp).name(), cmd_resp);
            return E_DEFAULT;
        }

        if (dbc::network::connection_manager::instance().broadcast_message(node_req_msg) != E_SUCCESS) {
            auto cmd_resp = std::make_shared<::cmd_list_node_rsp>();
            cmd_resp->result = E_DEFAULT;
            cmd_resp->result_info = "broadcast failed";
            cmd_resp->header.__set_session_id(cmd_req_msg->header.session_id);

            topic_manager::instance().publish<void>(typeid(::cmd_list_node_rsp).name(), cmd_resp);
            return E_DEFAULT;
        }
    }

    return E_SUCCESS;
}

int32_t cmd_request_service::on_node_query_node_info_rsp(std::shared_ptr<dbc::network::message> &msg) {
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

int32_t cmd_request_service::on_node_query_node_info_timer(const std::shared_ptr<core_timer>& timer)
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


int32_t cmd_request_service::validate_cmd_training_task_conf(const bpo::variables_map &vm, std::string &error) {
    std::string params[] = {
            "training_engine",
            "entry_file",
            "code_dir"
    };

    for (const auto &param : params) {
        if (0 == vm.count(param)) {
            error = "missing param:" + param;
            return E_DEFAULT;
        }
    }

    std::string engine_name = vm["training_engine"].as<std::string>();
    if (engine_name.empty() || engine_name.size() > MAX_ENGINE_IMGE_NAME_LEN) {
        error = "training_engine name length exceed maximum ";
        return E_DEFAULT;
    }

    std::string entry_file_name = vm["entry_file"].as<std::string>();
    if (entry_file_name.empty() || entry_file_name.size() > MAX_ENTRY_FILE_NAME_LEN) {
        error = "entry_file name length exceed maximum ";
        return E_DEFAULT;
    }

    std::string code_dir = vm["code_dir"].as<std::string>();
    if (code_dir.empty() || E_SUCCESS != validate_ipfs_path(code_dir)) {
        error = "code_dir path is not valid";
        return E_DEFAULT;
    }

    if (0 != vm.count("data_dir") && !vm["data_dir"].as<std::string>().empty()) {
        std::string data_dir = vm["data_dir"].as<std::string>();
        if (E_SUCCESS != validate_ipfs_path(data_dir)) {
            error = "data_dir path is not valid";
            return E_DEFAULT;
        }
    }

    if (0 == vm.count("peer_nodes_list") || vm["peer_nodes_list"].as<std::vector<std::string>>().empty()) {
        if (code_dir != TASK_RESTART) {
            error = "missing param:peer_nodes_list";
            return E_DEFAULT;
        }
    } else {
        std::vector<std::string> nodes;
        nodes = vm["peer_nodes_list"].as<std::vector<std::string>>();
        for (auto &node_id : nodes) {
            if (node_id.empty())
                continue;
        }
    }

    return E_SUCCESS;
}

int32_t cmd_request_service::validate_ipfs_path(const std::string &path_arg) {
    if (path_arg == std::string(NODE_REBOOT) || path_arg == std::string(TASK_RESTART)) {
        return E_SUCCESS;
    }

    cregex reg = cregex::compile("(^[a-zA-Z0-9]{46}$)");
    if (regex_match(path_arg.c_str(), reg)) {
        return E_SUCCESS;
    }
    return E_DEFAULT;
}

int32_t cmd_request_service::validate_entry_file_name(const std::string &entry_file_name) {
    /*cregex reg = cregex::compile("(^[0-9a-zA-Z]{1,}[-_]{0,}[0-9a-zA-Z]{0,}.py$)");
    if (regex_match(entry_file_name.c_str(), reg))
    {
        return E_SUCCESS;
    }*/
    //return E_DEFAULT;
    if (entry_file_name.size() > MAX_ENTRY_FILE_NAME_LEN) {
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

bool cmd_request_service::check_nonce(const std::string& nonce) {
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

bool cmd_request_service::check_req_header(std::shared_ptr<dbc::network::message> &msg) {
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

bool cmd_request_service::check_rsp_header(std::shared_ptr<dbc::network::message> &msg) {
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

int32_t cmd_request_service::on_binary_forward(std::shared_ptr<dbc::network::message> &msg) {
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
