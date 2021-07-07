#include "cmd_request_service.h"
#include <cassert>
#include "server.h"
#include "conf_manager.h"
#include "service_message_id.h"
#include "matrix_types.h"
#include "matrix_coder.h"
#include "ip_validator.h"
#include <boost/exception/all.hpp>
#include "crypt_util.h"
#include "task_common_def.h"
#include <boost/xpressive/xpressive_dynamic.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>
#include "message_id.h"

using namespace matrix::core;

namespace dbc {
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
        // destroy task
        SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_destroy_task_req).name())
        SUBSCRIBE_BUS_MESSAGE(NODE_DESTROY_TASK_RSP)
        // task logs
        SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_task_logs_req).name())
        SUBSCRIBE_BUS_MESSAGE(NODE_TASK_LOGS_RSP)
        // list task
        SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_list_task_req).name())
        SUBSCRIBE_BUS_MESSAGE(NODE_LIST_TASK_RSP)

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
        // destroy task
        BIND_MESSAGE_INVOKER(typeid(::cmd_destroy_task_req).name(), &cmd_request_service::on_cmd_destroy_task_req)
        BIND_MESSAGE_INVOKER(NODE_DESTROY_TASK_RSP, &cmd_request_service::on_node_destroy_task_rsp)
        // task logs
        BIND_MESSAGE_INVOKER(typeid(::cmd_task_logs_req).name(), &cmd_request_service::on_cmd_task_logs_req);
        BIND_MESSAGE_INVOKER(NODE_TASK_LOGS_RSP, &cmd_request_service::on_node_task_logs_rsp);
        // list task
        BIND_MESSAGE_INVOKER(typeid(::cmd_list_task_req).name(), &cmd_request_service::on_cmd_list_task_req);
        BIND_MESSAGE_INVOKER(NODE_LIST_TASK_RSP, &cmd_request_service::on_node_list_task_rsp);

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
        m_timer_invokers[NODE_DESTROY_TASK_TIMER] = std::bind(&cmd_request_service::on_node_destroy_task_timer, this, std::placeholders::_1);
        // task logs
        m_timer_invokers[NODE_TASK_LOGS_TIMER] = std::bind(&cmd_request_service::on_node_task_logs_timer, this, std::placeholders::_1);
        // list task
        m_timer_invokers[NODE_LIST_TASK_TIMER] = std::bind(&cmd_request_service::on_node_list_task_timer, this, std::placeholders::_1);
    }

    // create task
    std::shared_ptr<dbc::network::message> cmd_request_service::create_node_create_task_req_msg(const std::shared_ptr<::cmd_create_task_req> &cmd_req) {
        if (cmd_req->peer_nodes_list.empty()) {
            LOG_ERROR << "create failed: no peer_nodes_list!";
            return nullptr;
        }

        auto req_content = std::make_shared<matrix::service_core::node_create_task_req>();
        // header
        req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
        req_content->header.__set_msg_name(NODE_CREATE_TASK_REQ);
        req_content->header.__set_nonce(dbc::create_nonce());
        req_content->header.__set_session_id(cmd_req->header.session_id);
        std::vector<std::string> path;
        path.push_back(CONF_MANAGER->get_node_id());
        req_content->header.__set_path(path);
        std::map<std::string, std::string> exten_info;
        std::string sign_message = req_content->header.nonce + cmd_req->additional;
        std::string signature = dbc::sign(sign_message, CONF_MANAGER->get_node_private_key());
        if (signature.empty())  return nullptr;
        exten_info["sign"] = signature;
        exten_info["sign_algo"] = ECDSA;
        exten_info["sign_at"] = boost::str(boost::format("%d") % std::time(nullptr));
        exten_info["origin_id"] = CONF_MANAGER->get_node_id();
        req_content->header.__set_exten_info(exten_info);
        // body
        req_content->body.__set_peer_nodes_list(cmd_req->peer_nodes_list);
        req_content->body.__set_additional(cmd_req->additional);

        std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
        req_msg->set_name(NODE_CREATE_TASK_REQ);
        req_msg->set_content(req_content);

        // 创建定时器
        uint32_t timer_id = this->add_timer(NODE_CREATE_TASK_TIMER, CONF_MANAGER->get_timer_dbc_request_in_millisecond(),
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

            TOPIC_MANAGER->publish<void>(typeid(::cmd_create_task_rsp).name(), cmd_rsp_msg);
            return E_DEFAULT;
        }

        if (CONNECTION_MANAGER->broadcast_message(node_req_msg) != E_SUCCESS) {
            std::shared_ptr<::cmd_create_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_create_task_rsp>();
            cmd_rsp_msg->result = E_DEFAULT;
            cmd_rsp_msg->result_info = "broadcast failed";
            cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);

            TOPIC_MANAGER->publish<void>(typeid(::cmd_create_task_rsp).name(), cmd_rsp_msg);
            return E_DEFAULT;
        }

        return E_SUCCESS;
    }

    int32_t cmd_request_service::on_node_create_task_rsp(std::shared_ptr<dbc::network::message> &msg) {
        if (!precheck_msg(msg)) {
            LOG_ERROR << "precheck_msg failed";
            return E_DEFAULT;
        }

        std::shared_ptr<matrix::service_core::node_create_task_rsp> rsp_content =
                std::dynamic_pointer_cast<matrix::service_core::node_create_task_rsp>(msg->content);
        if (!rsp_content) {
            LOG_ERROR << "rsp is nullptr";
            return E_DEFAULT;
        }

        std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id
                               + rsp_content->body.task_id + rsp_content->body.login_password;
        if (!dbc::verify_sign(rsp_content->header.exten_info["sign"], sign_msg,
                              rsp_content->header.exten_info["origin_id"])) {
            LOG_ERROR << "verify sign failed";
            return E_DEFAULT;
        }

        std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
        if (nullptr == session) {
            CONNECTION_MANAGER->send_resp_message(msg);
            return E_SUCCESS;
        }

        std::shared_ptr<::cmd_create_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_create_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(rsp_content->header.session_id);
        cmd_rsp_msg->result = rsp_content->body.result;
        cmd_rsp_msg->result_info = rsp_content->body.result_msg;
        cmd_rsp_msg->task_id = rsp_content->body.task_id;
        cmd_rsp_msg->login_password = rsp_content->body.login_password;

        TOPIC_MANAGER->publish<void>(typeid(::cmd_create_task_rsp).name(), cmd_rsp_msg);

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
            TOPIC_MANAGER->publish<void>(typeid(::cmd_create_task_rsp).name(), cmd_rsp);
            return E_DEFAULT;
        }

        const string &session_id = timer->get_session_id();
        std::shared_ptr<service_session> session = get_session(session_id);
        if (nullptr == session) {
            cmd_rsp->result = E_DEFAULT;
            cmd_rsp->result_info = "session is nullptr";
            TOPIC_MANAGER->publish<void>(typeid(::cmd_create_task_rsp).name(), cmd_rsp);
            return E_DEFAULT;
        }

        cmd_rsp->header.__set_session_id(session_id);
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "create task timeout";
        cmd_rsp->task_id = "";
        cmd_rsp->login_password = "";

        TOPIC_MANAGER->publish<void>(typeid(::cmd_create_task_rsp).name(), cmd_rsp);

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
        auto req_content = std::make_shared<matrix::service_core::node_start_task_req>();
        // header
        req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
        req_content->header.__set_msg_name(NODE_START_TASK_REQ);
        req_content->header.__set_nonce(dbc::create_nonce());
        req_content->header.__set_session_id(cmd_req->header.session_id);
        std::vector<std::string> path;
        path.push_back(CONF_MANAGER->get_node_id());
        req_content->header.__set_path(path);
        std::map<std::string, std::string> exten_info;
        std::string sign_message = cmd_req->task_id + req_content->header.nonce + cmd_req->additional;
        std::string signature = dbc::sign(sign_message, CONF_MANAGER->get_node_private_key());
        if (signature.empty()) return nullptr;
        exten_info["sign"] = signature;
        exten_info["sign_algo"] = ECDSA;
        exten_info["sign_at"] = boost::str(boost::format("%d") % std::time(nullptr));
        exten_info["origin_id"] = CONF_MANAGER->get_node_id();
        req_content->header.__set_exten_info(exten_info);
        // body
        req_content->body.__set_task_id(cmd_req->task_id);
        req_content->body.__set_peer_nodes_list(cmd_req->peer_nodes_list);
        req_content->body.__set_additional(cmd_req->additional);

        std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
        req_msg->set_name(NODE_START_TASK_REQ);
        req_msg->set_content(req_content);

        // 创建定时器
        uint32_t timer_id = this->add_timer(NODE_START_TASK_TIMER, CONF_MANAGER->get_timer_dbc_request_in_millisecond(),
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

            TOPIC_MANAGER->publish<void>(typeid(::cmd_start_task_rsp).name(), cmd_rsp_msg);
            return E_DEFAULT;
        }

        if (CONNECTION_MANAGER->broadcast_message(node_req_msg) != E_SUCCESS) {
            std::shared_ptr<::cmd_start_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_start_task_rsp>();
            cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
            cmd_rsp_msg->result = E_DEFAULT;
            cmd_rsp_msg->result_info = "submit error. pls check network";

            TOPIC_MANAGER->publish<void>(typeid(::cmd_start_task_rsp).name(), cmd_rsp_msg);
            return E_DEFAULT;
        }

        return E_SUCCESS;
    }

    int32_t cmd_request_service::on_node_start_task_rsp(std::shared_ptr<dbc::network::message> &msg) {
        if (!precheck_msg(msg)) {
            LOG_ERROR << "precheck_msg failed";
            return E_DEFAULT;
        }

        std::shared_ptr<matrix::service_core::node_start_task_rsp> rsp_content = std::dynamic_pointer_cast<matrix::service_core::node_start_task_rsp>(msg->content);
        if (!rsp_content) {
            LOG_ERROR << "rsp is nullptr";
            return E_DEFAULT;
        }

        std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
        if (!dbc::verify_sign(rsp_content->header.exten_info["sign"], sign_msg, rsp_content->header.exten_info["origin_id"])) {
            LOG_ERROR << "verify sign failed";
            return E_DEFAULT;
        }

        std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
        if (nullptr == session) {
            CONNECTION_MANAGER->send_resp_message(msg);
            return E_SUCCESS;
        }

        std::shared_ptr<::cmd_start_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_start_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(rsp_content->header.session_id);
        cmd_rsp_msg->result = rsp_content->body.result;
        cmd_rsp_msg->result_info = rsp_content->body.result_msg;

        TOPIC_MANAGER->publish<void>(typeid(::cmd_start_task_rsp).name(), cmd_rsp_msg);

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
            TOPIC_MANAGER->publish<void>(typeid(::cmd_start_task_rsp).name(), cmd_rsp);
            return E_DEFAULT;
        }

        const string &session_id = timer->get_session_id();
        std::shared_ptr<service_session> session = get_session(session_id);
        if (nullptr == session) {
            cmd_rsp->result = E_DEFAULT;
            cmd_rsp->result_info = "session is nullptr";
            TOPIC_MANAGER->publish<void>(typeid(::cmd_start_task_rsp).name(), cmd_rsp);
            return E_DEFAULT;
        }

        cmd_rsp->header.__set_session_id(session_id);
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "start task timeout";

        TOPIC_MANAGER->publish<void>(typeid(::cmd_start_task_rsp).name(), cmd_rsp);

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
        std::shared_ptr<matrix::service_core::node_stop_task_req> req_content = std::make_shared<matrix::service_core::node_stop_task_req>();
        // header
        req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
        req_content->header.__set_msg_name(NODE_STOP_TASK_REQ);
        req_content->header.__set_nonce(dbc::create_nonce());
        req_content->header.__set_session_id(cmd_req->header.session_id);
        std::vector<std::string> path;
        path.push_back(CONF_MANAGER->get_node_id());
        req_content->header.__set_path(path);
        std::map<std::string, std::string> exten_info;
        std::string sign_msg = cmd_req->task_id + req_content->header.nonce + cmd_req->additional;
        std::string signature = dbc::sign(sign_msg, CONF_MANAGER->get_node_private_key());
        if (signature.empty()) return nullptr;
        exten_info["sign"] = signature;
        exten_info["sign_algo"] = ECDSA;
        exten_info["sign_at"] = boost::str(boost::format("%d") % std::time(nullptr));
        exten_info["origin_id"] = CONF_MANAGER->get_node_id();
        req_content->header.__set_exten_info(exten_info);
        // body
        req_content->body.__set_task_id(cmd_req->task_id);
        req_content->body.__set_peer_nodes_list(cmd_req->peer_nodes_list);
        req_content->body.__set_additional(cmd_req->additional);

        std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
        req_msg->set_name(NODE_STOP_TASK_REQ);
        req_msg->set_content(req_content);

        // 创建定时器
        uint32_t timer_id = this->add_timer(NODE_STOP_TASK_TIMER, CONF_MANAGER->get_timer_dbc_request_in_millisecond(),
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

            TOPIC_MANAGER->publish<void>(typeid(::cmd_stop_task_rsp).name(), cmd_rsp_msg);
            return E_DEFAULT;
        }

        if (E_SUCCESS != CONNECTION_MANAGER->broadcast_message(task_req_msg)) {
            std::shared_ptr<::cmd_stop_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_stop_task_rsp>();
            cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
            cmd_rsp_msg->result = E_INACTIVE_CHANNEL;
            cmd_rsp_msg->result_info = "dbc node don't connect to network, pls check ";

            TOPIC_MANAGER->publish<void>(typeid(::cmd_stop_task_rsp).name(), cmd_rsp_msg);
            return E_DEFAULT;
        }

        return E_SUCCESS;
    }

    int32_t cmd_request_service::on_node_stop_task_rsp(std::shared_ptr<dbc::network::message> &msg) {
        if (!precheck_msg(msg)) {
            LOG_ERROR << "precheck_msg failed";
            return E_DEFAULT;
        }

        std::shared_ptr<matrix::service_core::node_stop_task_rsp> rsp_content = std::dynamic_pointer_cast<matrix::service_core::node_stop_task_rsp>(msg->content);
        if (!rsp_content) {
            LOG_ERROR << "rsp is nullptr";
            return E_DEFAULT;
        }

        std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
        if (!dbc::verify_sign(rsp_content->header.exten_info["sign"], sign_msg, rsp_content->header.exten_info["origin_id"])) {
            LOG_ERROR << "verify sign failed";
            return E_DEFAULT;
        }

        std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
        if (nullptr == session) {
            CONNECTION_MANAGER->send_resp_message(msg);
            return E_SUCCESS;
        }

        std::shared_ptr<::cmd_stop_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_stop_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(rsp_content->header.session_id);
        cmd_rsp_msg->result = rsp_content->body.result;
        cmd_rsp_msg->result_info = rsp_content->body.result_msg;

        TOPIC_MANAGER->publish<void>(typeid(::cmd_stop_task_rsp).name(), cmd_rsp_msg);

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
            TOPIC_MANAGER->publish<void>(typeid(::cmd_stop_task_rsp).name(), cmd_rsp);
            return E_DEFAULT;
        }

        const string &session_id = timer->get_session_id();
        std::shared_ptr<service_session> session = get_session(session_id);
        if (nullptr == session) {
            cmd_rsp->result = E_DEFAULT;
            cmd_rsp->result_info = "session is nullptr";
            TOPIC_MANAGER->publish<void>(typeid(::cmd_stop_task_rsp).name(), cmd_rsp);
            return E_DEFAULT;
        }

        cmd_rsp->header.__set_session_id(session_id);
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "stop task timeout";

        TOPIC_MANAGER->publish<void>(typeid(::cmd_stop_task_rsp).name(), cmd_rsp);

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
        auto req_content = std::make_shared<matrix::service_core::node_restart_task_req>();
        // header
        req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
        req_content->header.__set_msg_name(NODE_RESTART_TASK_REQ);
        req_content->header.__set_nonce(dbc::create_nonce());
        req_content->header.__set_session_id(cmd_req->header.session_id);
        std::vector<std::string> path;
        path.push_back(CONF_MANAGER->get_node_id());
        req_content->header.__set_path(path);
        std::map<std::string, std::string> exten_info;
        std::string sign_message = cmd_req->task_id + req_content->header.nonce + cmd_req->additional;
        std::string signature = dbc::sign(sign_message, CONF_MANAGER->get_node_private_key());
        if (signature.empty()) return nullptr;
        exten_info["sign"] = signature;
        exten_info["sign_algo"] = ECDSA;
        exten_info["sign_at"] = boost::str(boost::format("%d") % std::time(nullptr));
        exten_info["origin_id"] = CONF_MANAGER->get_node_id();
        req_content->header.__set_exten_info(exten_info);
        // body
        req_content->body.__set_task_id(cmd_req->task_id);
        req_content->body.__set_peer_nodes_list(cmd_req->peer_nodes_list);
        req_content->body.__set_additional(cmd_req->additional);

        std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
        req_msg->set_name(NODE_RESTART_TASK_REQ);
        req_msg->set_content(req_content);

        // 创建定时器
        uint32_t timer_id = this->add_timer(NODE_RESTART_TASK_TIMER, CONF_MANAGER->get_timer_dbc_request_in_millisecond(),
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

        std::shared_ptr<::cmd_restart_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_restart_task_rsp>();
        cmd_rsp_msg->result = E_SUCCESS;
        cmd_rsp_msg->result_info = "success";
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);

        auto task_req_msg = create_node_restart_task_req_msg(cmd_req_msg);
        if (nullptr == task_req_msg) {
            cmd_rsp_msg->result = E_DEFAULT;
            cmd_rsp_msg->result_info = "create node request failed!";
            TOPIC_MANAGER->publish<void>(typeid(::cmd_restart_task_rsp).name(), cmd_rsp_msg);
            return E_DEFAULT;
        }

        if (CONNECTION_MANAGER->broadcast_message(task_req_msg) != E_SUCCESS) {
            cmd_rsp_msg->result = E_DEFAULT;
            cmd_rsp_msg->result_info = "submit error. pls check network";
            TOPIC_MANAGER->publish<void>(typeid(::cmd_restart_task_rsp).name(), cmd_rsp_msg);
            return E_DEFAULT;
        }

        TOPIC_MANAGER->publish<void>(typeid(::cmd_restart_task_rsp).name(), cmd_rsp_msg);

        return E_SUCCESS;
    }

    int32_t cmd_request_service::on_node_restart_task_rsp(std::shared_ptr<dbc::network::message> &msg) {
        if (!precheck_msg(msg)) {
            LOG_ERROR << "precheck_msg failed";
            return E_DEFAULT;
        }

        std::shared_ptr<matrix::service_core::node_restart_task_rsp> rsp_content = std::dynamic_pointer_cast<matrix::service_core::node_restart_task_rsp>(msg->content);
        if (!rsp_content) {
            LOG_ERROR << "rsp is nullptr";
            return E_DEFAULT;
        }

        std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
        if (!dbc::verify_sign(rsp_content->header.exten_info["sign"], sign_msg, rsp_content->header.exten_info["origin_id"])) {
            LOG_ERROR << "verify sign failed";
            return E_DEFAULT;
        }

        std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
        if (nullptr == session) {
            CONNECTION_MANAGER->send_resp_message(msg);
            return E_SUCCESS;
        }

        std::shared_ptr<::cmd_restart_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_restart_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(rsp_content->header.session_id);
        cmd_rsp_msg->result = rsp_content->body.result;
        cmd_rsp_msg->result_info = rsp_content->body.result_msg;

        TOPIC_MANAGER->publish<void>(typeid(::cmd_restart_task_rsp).name(), cmd_rsp_msg);

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
            TOPIC_MANAGER->publish<void>(typeid(::cmd_restart_task_rsp).name(), cmd_rsp);
            return E_DEFAULT;
        }

        const string &session_id = timer->get_session_id();
        std::shared_ptr<service_session> session = get_session(session_id);
        if (nullptr == session) {
            cmd_rsp->result = E_DEFAULT;
            cmd_rsp->result_info = "session is nullptr";
            TOPIC_MANAGER->publish<void>(typeid(::cmd_restart_task_rsp).name(), cmd_rsp);
            return E_DEFAULT;
        }

        cmd_rsp->header.__set_session_id(session_id);
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "restart task timeout";

        TOPIC_MANAGER->publish<void>(typeid(::cmd_restart_task_rsp).name(), cmd_rsp);

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
        std::shared_ptr<matrix::service_core::node_reset_task_req> req_content = std::make_shared<matrix::service_core::node_reset_task_req>();
        // header
        req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
        req_content->header.__set_msg_name(NODE_RESET_TASK_REQ);
        req_content->header.__set_nonce(dbc::create_nonce());
        req_content->header.__set_session_id(cmd_req->header.session_id);
        std::vector<std::string> path;
        path.push_back(CONF_MANAGER->get_node_id());
        req_content->header.__set_path(path);
        std::map<std::string, std::string> exten_info;
        std::string sign_msg = cmd_req->task_id + req_content->header.nonce + cmd_req->additional;
        std::string signature = dbc::sign(sign_msg, CONF_MANAGER->get_node_private_key());
        if (signature.empty()) return nullptr;
        exten_info["sign"] = signature;
        exten_info["sign_algo"] = ECDSA;
        exten_info["sign_at"] = boost::str(boost::format("%d") % std::time(nullptr));
        exten_info["origin_id"] = CONF_MANAGER->get_node_id();
        req_content->header.__set_exten_info(exten_info);
        // body
        req_content->body.__set_task_id(cmd_req->task_id);
        req_content->body.__set_peer_nodes_list(cmd_req->peer_nodes_list);
        req_content->body.__set_additional(cmd_req->additional);

        std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
        req_msg->set_name(NODE_RESET_TASK_REQ);
        req_msg->set_content(req_content);

        // 创建定时器
        uint32_t timer_id = this->add_timer(NODE_RESET_TASK_TIMER, CONF_MANAGER->get_timer_dbc_request_in_millisecond(),
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

            TOPIC_MANAGER->publish<void>(typeid(::cmd_reset_task_rsp).name(), cmd_rsp_msg);
            return E_DEFAULT;
        }

        if (E_SUCCESS != CONNECTION_MANAGER->broadcast_message(task_req_msg)) {
            std::shared_ptr<::cmd_reset_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_reset_task_rsp>();
            cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
            cmd_rsp_msg->result = E_INACTIVE_CHANNEL;
            cmd_rsp_msg->result_info = "dbc node don't connect to network, pls check ";

            TOPIC_MANAGER->publish<void>(typeid(::cmd_reset_task_rsp).name(), cmd_rsp_msg);
            return E_DEFAULT;
        }

        return E_SUCCESS;
    }

    int32_t cmd_request_service::on_node_reset_task_rsp(std::shared_ptr<dbc::network::message> &msg) {
        if (!precheck_msg(msg)) {
            LOG_ERROR << "precheck_msg failed";
            return E_DEFAULT;
        }

        std::shared_ptr<matrix::service_core::node_reset_task_rsp> rsp_content = std::dynamic_pointer_cast<matrix::service_core::node_reset_task_rsp>(msg->content);
        if (!rsp_content) {
            LOG_ERROR << "rsp is nullptr";
            return E_DEFAULT;
        }

        std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
        if (!dbc::verify_sign(rsp_content->header.exten_info["sign"], sign_msg, rsp_content->header.exten_info["origin_id"])) {
            LOG_ERROR << "verify sign failed";
            return E_DEFAULT;
        }

        std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
        if (nullptr == session) {
            CONNECTION_MANAGER->send_resp_message(msg);
            return E_SUCCESS;
        }

        std::shared_ptr<::cmd_reset_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_reset_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(rsp_content->header.session_id);
        cmd_rsp_msg->result = rsp_content->body.result;
        cmd_rsp_msg->result_info = rsp_content->body.result_msg;

        TOPIC_MANAGER->publish<void>(typeid(::cmd_reset_task_rsp).name(), cmd_rsp_msg);

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
            TOPIC_MANAGER->publish<void>(typeid(::cmd_reset_task_rsp).name(), cmd_rsp);
            return E_DEFAULT;
        }

        const string &session_id = timer->get_session_id();
        std::shared_ptr<service_session> session = get_session(session_id);
        if (nullptr == session) {
            cmd_rsp->result = E_DEFAULT;
            cmd_rsp->result_info = "session is nullptr";
            TOPIC_MANAGER->publish<void>(typeid(::cmd_reset_task_rsp).name(), cmd_rsp);
            return E_DEFAULT;
        }

        cmd_rsp->header.__set_session_id(session_id);
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "reset task timeout";

        TOPIC_MANAGER->publish<void>(typeid(::cmd_reset_task_rsp).name(), cmd_rsp);

        session->clear();
        this->remove_session(session_id);
        return E_SUCCESS;
    }

    // destroy task
    std::shared_ptr<dbc::network::message> cmd_request_service::create_node_destroy_task_req_msg(const std::shared_ptr<::cmd_destroy_task_req> &cmd_req) {
        if (cmd_req->task_id.empty()) {
            LOG_ERROR << "restart failed: task_id is empty!";
            return nullptr;
        }

        if (cmd_req->peer_nodes_list.empty()) {
            LOG_ERROR << "restart failed: no peer_nodes_list!";
            return nullptr;
        }

        // 创建 node_ 请求
        std::shared_ptr<matrix::service_core::node_destroy_task_req> req_content = std::make_shared<matrix::service_core::node_destroy_task_req>();
        // header
        req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
        req_content->header.__set_msg_name(NODE_DESTROY_TASK_REQ);
        req_content->header.__set_nonce(dbc::create_nonce());
        req_content->header.__set_session_id(cmd_req->header.session_id);
        std::vector<std::string> path;
        path.push_back(CONF_MANAGER->get_node_id());
        req_content->header.__set_path(path);
        std::map<std::string, std::string> exten_info;
        std::string sign_msg = cmd_req->task_id + req_content->header.nonce + cmd_req->additional;
        std::string signature = dbc::sign(sign_msg, CONF_MANAGER->get_node_private_key());
        if (signature.empty()) return nullptr;
        exten_info["sign"] = signature;
        exten_info["sign_algo"] = ECDSA;
        exten_info["sign_at"] = boost::str(boost::format("%d") % std::time(nullptr));
        exten_info["origin_id"] = CONF_MANAGER->get_node_id();
        req_content->header.__set_exten_info(exten_info);
        // body
        req_content->body.__set_task_id(cmd_req->task_id);
        req_content->body.__set_peer_nodes_list(cmd_req->peer_nodes_list);
        req_content->body.__set_additional(cmd_req->additional);

        std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
        req_msg->set_name(NODE_DESTROY_TASK_REQ);
        req_msg->set_content(req_content);

        // 创建定时器
        uint32_t timer_id = this->add_timer(NODE_DESTROY_TASK_TIMER, CONF_MANAGER->get_timer_dbc_request_in_millisecond(),
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

    int32_t cmd_request_service::on_cmd_destroy_task_req(const std::shared_ptr<dbc::network::message> &msg) {
        std::shared_ptr<::cmd_destroy_task_req> cmd_req_msg = std::dynamic_pointer_cast<::cmd_destroy_task_req>(
                msg->get_content());
        if (cmd_req_msg == nullptr) {
            LOG_ERROR << "cmd_req_msg is null";
            return E_NULL_POINTER;
        }

        auto task_req_msg = create_node_destroy_task_req_msg(cmd_req_msg);
        if (nullptr == task_req_msg) {
            std::shared_ptr<::cmd_destroy_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_destroy_task_rsp>();
            cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
            cmd_rsp_msg->result = E_DEFAULT;
            cmd_rsp_msg->result_info = "create node request failed!";

            TOPIC_MANAGER->publish<void>(typeid(::cmd_destroy_task_rsp).name(), cmd_rsp_msg);
            return E_DEFAULT;
        }

        if (E_SUCCESS != CONNECTION_MANAGER->broadcast_message(task_req_msg)) {
            std::shared_ptr<::cmd_destroy_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_destroy_task_rsp>();
            cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);
            cmd_rsp_msg->result = E_DEFAULT;
            cmd_rsp_msg->result_info = "dbc node don't connect to network, pls check ";

            TOPIC_MANAGER->publish<void>(typeid(::cmd_destroy_task_rsp).name(), cmd_rsp_msg);
            return E_DEFAULT;
        }

        return E_SUCCESS;
    }

    int32_t cmd_request_service::on_node_destroy_task_rsp(std::shared_ptr<dbc::network::message> &msg) {
        if (!precheck_msg(msg)) {
            LOG_ERROR << "precheck_msg failed";
            return E_DEFAULT;
        }

        std::shared_ptr<matrix::service_core::node_destroy_task_rsp> rsp_content = std::dynamic_pointer_cast<matrix::service_core::node_destroy_task_rsp>(msg->content);
        if (!rsp_content) {
            LOG_ERROR << "rsp is nullptr";
            return E_DEFAULT;
        }

        std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id;
        if (!dbc::verify_sign(rsp_content->header.exten_info["sign"], sign_msg, rsp_content->header.exten_info["origin_id"])) {
            LOG_ERROR << "verify sign failed";
            return E_DEFAULT;
        }

        std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
        if (nullptr == session) {
            CONNECTION_MANAGER->send_resp_message(msg);
            return E_SUCCESS;
        }

        std::shared_ptr<::cmd_destroy_task_rsp> cmd_rsp_msg = std::make_shared<::cmd_destroy_task_rsp>();
        cmd_rsp_msg->header.__set_session_id(rsp_content->header.session_id);
        cmd_rsp_msg->result = rsp_content->body.result;
        cmd_rsp_msg->result_info = rsp_content->body.result_msg;

        TOPIC_MANAGER->publish<void>(typeid(::cmd_destroy_task_rsp).name(), cmd_rsp_msg);

        this->remove_timer(session->get_timer_id());
        session->clear();
        this->remove_session(session->get_session_id());
        return E_SUCCESS;
    }

    int32_t cmd_request_service::on_node_destroy_task_timer(const std::shared_ptr<core_timer> &timer) {
        std::shared_ptr<::cmd_destroy_task_rsp> cmd_rsp = std::make_shared<::cmd_destroy_task_rsp>();
        if (nullptr == timer) {
            cmd_rsp->result = E_DEFAULT;
            cmd_rsp->result_info = "timer is nullptr";
            TOPIC_MANAGER->publish<void>(typeid(::cmd_destroy_task_rsp).name(), cmd_rsp);
            return E_DEFAULT;
        }

        const string &session_id = timer->get_session_id();
        std::shared_ptr<service_session> session = get_session(session_id);
        if (nullptr == session) {
            cmd_rsp->result = E_DEFAULT;
            cmd_rsp->result_info = "session is nullptr";
            TOPIC_MANAGER->publish<void>(typeid(::cmd_destroy_task_rsp).name(), cmd_rsp);
            return E_DEFAULT;
        }

        cmd_rsp->header.__set_session_id(session_id);
        cmd_rsp->result = E_DEFAULT;
        cmd_rsp->result_info = "destroy task timeout";

        TOPIC_MANAGER->publish<void>(typeid(::cmd_destroy_task_rsp).name(), cmd_rsp);

        session->clear();
        this->remove_session(session_id);
        return E_SUCCESS;
    }

    // task logs
    std::shared_ptr<dbc::network::message>
    cmd_request_service::create_node_task_logs_req_msg(const std::shared_ptr<::cmd_task_logs_req> &cmd_req) {
        // check
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

        auto req_content = std::make_shared<matrix::service_core::node_task_logs_req>();
        req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
        req_content->header.__set_msg_name(NODE_TASK_LOGS_REQ);
        req_content->header.__set_nonce(dbc::create_nonce());
        req_content->header.__set_session_id(cmd_req->header.session_id);
        std::vector<std::string> path;
        path.push_back(CONF_MANAGER->get_node_id());
        req_content->header.__set_path(path);
        req_content->body.__set_task_id(cmd_req->task_id);
        req_content->body.__set_head_or_tail(cmd_req->head_or_tail);
        req_content->body.__set_number_of_lines(cmd_req->number_of_lines);
        req_content->body.__set_peer_nodes_list(cmd_req->peer_nodes_list);
        req_content->body.__set_additional(cmd_req->additional);

        //创建签名
        std::map<std::string, std::string> exten_info;
        exten_info["origin_id"] = CONF_MANAGER->get_node_id();
        for (auto &peer_node : req_content->body.peer_nodes_list) {
            exten_info["dest_id"] += peer_node + " ";
        }

        std::string sig_msg = req_content->body.task_id + req_content->header.nonce + req_content->header.session_id +
                req_content->body.additional;
        std::string signature = dbc::sign(sig_msg, CONF_MANAGER->get_node_private_key());
        exten_info["sign"] = signature;
        exten_info["sign_algo"] = ECDSA;
        time_t cur = std::time(nullptr);
        exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
        req_content->header.__set_exten_info(exten_info);

        std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
        req_msg->set_name(NODE_TASK_LOGS_REQ);
        req_msg->set_content(req_content);

        // 创建定时器
        uint32_t timer_id = this->add_timer(NODE_TASK_LOGS_TIMER, CONF_MANAGER->get_timer_dbc_request_in_millisecond(),
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

        std::shared_ptr<::cmd_task_logs_rsp> cmd_rsp_msg = std::make_shared<::cmd_task_logs_rsp>();
        cmd_rsp_msg->result = E_SUCCESS;
        cmd_rsp_msg->result_info = "success";
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);

        // 创建 node_ 请求
        auto task_req_msg = create_node_task_logs_req_msg(cmd_req_msg);
        if (nullptr == task_req_msg) {
            cmd_rsp_msg->result = E_DEFAULT;
            cmd_rsp_msg->result_info = "create node request failed!";
            TOPIC_MANAGER->publish<void>(typeid(::cmd_task_logs_rsp).name(), cmd_rsp_msg);
            return E_DEFAULT;
        }

        if (E_SUCCESS != CONNECTION_MANAGER->broadcast_message(task_req_msg)) {
            cmd_rsp_msg->result = E_DEFAULT;
            cmd_rsp_msg->result_info = "dbc node don't connect to network, pls check ";
            TOPIC_MANAGER->publish<void>(typeid(::cmd_task_logs_rsp).name(), cmd_rsp_msg);
            return E_DEFAULT;
        }

        return E_SUCCESS;
    }

    int32_t cmd_request_service::on_node_task_logs_rsp(std::shared_ptr<dbc::network::message> &msg) {
        if (!precheck_msg(msg)) {
            LOG_ERROR << "precheck_msg failed";
            return E_DEFAULT;
        }

        std::shared_ptr<matrix::service_core::node_task_logs_rsp> rsp_content =
                std::dynamic_pointer_cast<matrix::service_core::node_task_logs_rsp>(msg->content);
        if (!rsp_content) {
            LOG_ERROR << "rsp is nullptr";
            return E_DEFAULT;
        }

        std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id
                    + rsp_content->body.log_content;
        if (!dbc::verify_sign(rsp_content->header.exten_info["sign"], sign_msg, rsp_content->header.exten_info["origin_id"])) {
            LOG_ERROR << "verify sign failed";
            return E_DEFAULT;
        }

        std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
        if (nullptr == session) {
            CONNECTION_MANAGER->send_resp_message(msg);
            return E_SUCCESS;
        }

        std::shared_ptr<::cmd_task_logs_rsp> cmd_rsp_msg = std::make_shared<::cmd_task_logs_rsp>();
        cmd_rsp_msg->header.__set_session_id(rsp_content->header.session_id);
        cmd_rsp_msg->result = rsp_content->body.result;
        cmd_rsp_msg->result_info = rsp_content->body.result_msg;
        cmd_rsp_msg->log_content = rsp_content->body.log_content;

        TOPIC_MANAGER->publish<void>(typeid(::cmd_task_logs_rsp).name(), cmd_rsp_msg);

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
            TOPIC_MANAGER->publish<void>(typeid(::cmd_task_logs_rsp).name(), cmd_resp);
            return E_NULL_POINTER;
        }

        const string &session_id = timer->get_session_id();
        std::shared_ptr<service_session> session = get_session(session_id);
        if (nullptr == session) {
            cmd_resp->result = E_DEFAULT;
            cmd_resp->result_info = "session is nullptr";
            TOPIC_MANAGER->publish<void>(typeid(::cmd_task_logs_rsp).name(), cmd_resp);
            return E_DEFAULT;
        }

        cmd_resp->header.__set_session_id(session_id);
        cmd_resp->result = E_DEFAULT;
        cmd_resp->result_info = "get task log time out";

        TOPIC_MANAGER->publish<void>(typeid(::cmd_task_logs_rsp).name(), cmd_resp);

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

        auto req_content = std::make_shared<matrix::service_core::node_list_task_req>();
        req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
        req_content->header.__set_msg_name(NODE_LIST_TASK_REQ);
        req_content->header.__set_nonce(dbc::create_nonce());
        req_content->header.__set_session_id(cmd_req->header.session_id);
        std::vector<std::string> path;
        path.push_back(CONF_MANAGER->get_node_id());
        req_content->header.__set_path(path);

        if (!cmd_req->task_id.empty()) {
            req_content->body.__set_task_id(cmd_req->task_id);
        } else {
            req_content->body.__set_task_id("");
        }
        req_content->body.__set_peer_nodes_list(cmd_req->peer_nodes_list);
        req_content->body.__set_additional(cmd_req->additional);

        std::map<std::string, std::string> exten_info;
        exten_info["origin_id"] = CONF_MANAGER->get_node_id();

        std::string sign_message = req_content->body.task_id + req_content->header.nonce +
                req_content->header.session_id + req_content->body.additional;
        std::string sign = dbc::sign(sign_message, CONF_MANAGER->get_node_private_key());
        exten_info["sign"] = sign;
        exten_info["sign_algo"] = ECDSA;
        time_t cur = std::time(nullptr);
        exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
        req_content->header.__set_exten_info(exten_info);

        std::shared_ptr<dbc::network::message> req_msg = std::make_shared<dbc::network::message>();
        req_msg->set_name(NODE_LIST_TASK_REQ);
        req_msg->set_content(req_content);

        //add timer
        uint32_t timer_id = add_timer(NODE_LIST_TASK_TIMER, CONF_MANAGER->get_timer_dbc_request_in_millisecond(),
                                      ONLY_ONE_TIME, req_content->header.session_id);

        std::shared_ptr<service_session> session = std::make_shared<service_session>(timer_id,
                                                                                     req_content->header.session_id);

        //session context
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

        auto cmd_rsp_msg = std::make_shared<::cmd_list_task_rsp>();
        cmd_rsp_msg->result = E_SUCCESS;
        cmd_rsp_msg->result_info = "success";
        cmd_rsp_msg->header.__set_session_id(cmd_req_msg->header.session_id);

        // 创建 node_ 请求
        auto task_req_msg = create_node_list_task_req_msg(cmd_req_msg);
        if (nullptr == task_req_msg) {
            cmd_rsp_msg->result = E_DEFAULT;
            cmd_rsp_msg->result_info = "create node request failed!";
            TOPIC_MANAGER->publish<void>(typeid(::cmd_list_task_rsp).name(), cmd_rsp_msg);
            return E_DEFAULT;
        }

        if (E_SUCCESS != CONNECTION_MANAGER->broadcast_message(task_req_msg)) {
            cmd_rsp_msg->result = E_DEFAULT;
            cmd_rsp_msg->result_info = "dbc node don't connect to network, pls check ";
            TOPIC_MANAGER->publish<void>(typeid(::cmd_list_task_rsp).name(), cmd_rsp_msg);
            return E_DEFAULT;
        }

        return E_SUCCESS;
    }

    int32_t cmd_request_service::on_node_list_task_rsp(std::shared_ptr<dbc::network::message> &msg) {
        if (!precheck_msg(msg)) {
            LOG_ERROR << "msg precheck fail";
            return E_DEFAULT;
        }

        std::shared_ptr<matrix::service_core::node_list_task_rsp> rsp_content =
                std::dynamic_pointer_cast<matrix::service_core::node_list_task_rsp>(msg->content);
        if (!rsp_content) {
            LOG_ERROR << "recv list_training_resp but ctn is nullptr";
            return E_DEFAULT;
        }

        //get session
        std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
        if (nullptr == session) {
            LOG_ERROR << "ai power requester service get session null: " << rsp_content->header.session_id;

            //broadcast resp
            CONNECTION_MANAGER->send_resp_message(msg, msg->header.src_sid);

            return E_DEFAULT;
        }

        variables_map &vm = session->get_context().get_args();
        std::shared_ptr<dbc::network::message> req_msg = vm["req_msg"].as<std::shared_ptr<dbc::network::message>>();

        std::shared_ptr<::cmd_list_task_rsp> cmd_resp = std::make_shared<::cmd_list_task_rsp>();
        auto req_content = std::dynamic_pointer_cast<matrix::service_core::node_list_task_req>(req_msg->get_content());
        if (nullptr != req_content)
            cmd_resp->header.__set_session_id(req_content->header.session_id);
        cmd_resp->result = E_SUCCESS;
        cmd_resp->result_info = "";

        for (auto& ts : rsp_content->body.task_status_list) {
            ::cmd_task_status cts;
            cts.task_id = ts.task_id;
            cts.status = ts.status;
            cts.pwd = ts.pwd;
            cmd_resp->task_status_list.push_back(std::move(cts));
        }

        TOPIC_MANAGER->publish<void>(typeid(::cmd_list_task_rsp).name(), cmd_resp);

        this->remove_timer(session->get_timer_id());

        session->clear();
        this->remove_session(session->get_session_id());

        return E_SUCCESS;
    }

    int32_t cmd_request_service::on_node_list_task_timer(const std::shared_ptr<core_timer> &timer) {
        std::shared_ptr<::cmd_list_task_rsp> cmd_resp = std::make_shared<::cmd_list_task_rsp>();
        cmd_resp->result = E_DEFAULT;
        cmd_resp->result_info = "";

        if (nullptr == timer) {
            cmd_resp->result = E_DEFAULT;
            cmd_resp->result_info = "null ptr of timer.";
            TOPIC_MANAGER->publish<void>(typeid(::cmd_list_task_rsp).name(), cmd_resp);
            return E_NULL_POINTER;
        }

        std::string session_id = timer->get_session_id();
        std::shared_ptr<service_session> session = get_session(session_id);
        if (nullptr == session) {
            cmd_resp->result = E_DEFAULT;
            cmd_resp->result_info = "ai power requester service list training timer get session null: " + session_id;
            TOPIC_MANAGER->publish<void>(typeid(::cmd_list_task_rsp).name(), cmd_resp);
            return E_DEFAULT;
        }

        variables_map &vm = session->get_context().get_args();
        std::shared_ptr<dbc::network::message> req_msg = vm["req_msg"].as<std::shared_ptr<dbc::network::message>>();
        auto req_content = std::dynamic_pointer_cast<matrix::service_core::node_list_task_req>(req_msg->get_content());
        if (req_content != nullptr)
            cmd_resp->header.__set_session_id(req_content->header.session_id);

        TOPIC_MANAGER->publish<void>(typeid(::cmd_list_task_rsp).name(), cmd_resp);
        if (session) {
            LOG_DEBUG << "ai power requester service list training timer time out remove session: " << session_id;
            session->clear();
            this->remove_session(session_id);
        }

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

    bool cmd_request_service::precheck_msg(std::shared_ptr<dbc::network::message> &msg) {
        if (!msg) {
            LOG_ERROR << "msg is nullptr";
            return false;
        }

        std::shared_ptr<dbc::network::msg_base> base = msg->content;
        if (!base) {
            LOG_ERROR << "containt is nullptr";
            return false;
        }

        if (!dbc::check_id(base->header.nonce)) {
            LOG_ERROR << "nonce error ";
            return false;
        }

        if (!dbc::check_id(base->header.session_id)) {
            LOG_ERROR << "ai power requster service on_list_training_resp. session_id error ";
            return false;
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
            msg->content->header.path.push_back(CONF_MANAGER->get_node_id());

            LOG_INFO << "broadcast_message binary forward msg";
            CONNECTION_MANAGER->broadcast_message(msg);
        } else {
            CONNECTION_MANAGER->send_resp_message(msg);
        }

        return E_SUCCESS;
    }
}
