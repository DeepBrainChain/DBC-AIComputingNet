/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : rest_handler.cpp
* description       : rest callback function
* date              : 2018.11.9
* author            : tower && taz
**********************************************************************************/
#include "rest_handler.h"
#include "peer_candidate.h"
#include "task_common_def.h"
#include "log.h"
#include "json_util.h"
#include "rpc_error.h"
#include "server.h"
#include "rest_api_service.h"
#include "cmd_message.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"
#include "rapidjson/error/error.h"

/*
 * Unified successful response method
 * */
#define ERROR_REPLY(status, error_code, error_message) httpReq->reply_comm_rest_err(status,error_code,error_message);

/*
 * Unified error response method
 * */
#define SUCC_REPLY(data) httpReq->reply_comm_rest_succ(data);

/*
 * Initialize the context information that handles a response
 * */
#define INIT_RSP_CONTEXT(CMD_REQ, CMD_RSP) \
std::shared_ptr<dbc::network::http_request>& httpReq=hreq_context->m_hreq;\
std::shared_ptr<dbc::network::message>& req_msg=hreq_context->m_req_msg;\
std::shared_ptr<CMD_REQ> req = std::dynamic_pointer_cast<CMD_REQ>(req_msg->content);\
std::shared_ptr<CMD_RSP> resp = std::dynamic_pointer_cast<CMD_RSP>(resp_msg->content);\
rapidjson::Document document;\
rapidjson::Document::AllocatorType& allocator = document.GetAllocator();\
rapidjson::Value data(rapidjson::kObjectType);

namespace dbc {
    static void fill_json_filed_string(rapidjson::Value &data, rapidjson::Document::AllocatorType &allocator,
                                       const std::string& key, const std::string &val) {
        if (val.empty()) return;

        std::string tmp_val(val);
        tmp_val = string_util::rtrim(tmp_val, '\n');
        string_util::trim(tmp_val);

        data.AddMember(STRING_DUP(key), STRING_DUP(tmp_val), allocator);
    }

    // /tasks/
    std::shared_ptr<dbc::network::message> rest_task(const dbc::network::HTTP_REQUEST_PTR &httpReq, const std::string &path) {
        std::vector<std::string> path_list;
        rest_util::split_path(path, path_list);

        // GET /tasks 列出所有task列表
        if (path_list.empty()) {
            return rest_list_task(httpReq, path);
        }

        if (path_list.size() == 1) {
            const std::string &first_param = path_list[0];
            // create
            if (first_param == "start") {
                return rest_create_task(httpReq, path);
            }

            return rest_list_task(httpReq, path);
        }

        if (path_list.size() == 2) {
            const std::string &second_param = path_list[1];
            // start <task_id>
            if (second_param == "start") {
                return rest_start_task(httpReq, path);
            }

            // /tasks/<task_id>/restart
            if (second_param == "restart") {
                return rest_restart_task(httpReq, path);
            }

            // /tasks/<task_id>/stop 停止某个task
            if (second_param == "stop") {
                return rest_stop_task(httpReq, path);
            }

            // /tasks/<task_id>/reset
            if (second_param == "reset") {
                return rest_reset_task(httpReq, path);
            }

            // /tasks/<task_id>/destroy
            if (second_param == "delete") {
                return rest_delete_task(httpReq, path);
            }

            // /tasks/<task_id>/logs
            if (second_param == "logs") {
                return rest_task_logs(httpReq, path);
            }

            // /tasks/<task_id>/modify
            if (second_param == "modify") {
                return rest_modify_task(httpReq, path);
            }
        }

        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "invalid requests uri")
        return nullptr;
    }

    // create
    std::shared_ptr<dbc::network::message> rest_create_task(const dbc::network::HTTP_REQUEST_PTR &httpReq, const std::string &path) {
        if (httpReq->get_request_method() != dbc::network::http_request::POST) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST,
                        "Only support POST requests. POST /api/v1/tasks/start")
            return nullptr;
        }

        std::vector<std::string> path_list;
        rest_util::split_path(path, path_list);
        if (path_list.size() != 1) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid uri. Use /api/v1/tasks/start")
            return nullptr;
        }

        std::shared_ptr<cmd_create_task_req> cmd_req = std::make_shared<cmd_create_task_req>();

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
        msg->set_name(typeid(cmd_create_task_req).name());
        msg->set_content(cmd_req);
        return msg;
    }

    int32_t on_cmd_create_task_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg) {
        INIT_RSP_CONTEXT(cmd_create_task_req, cmd_create_task_rsp)

        if (resp->result != 0) {
            ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info)
            return E_DEFAULT;
        }

        data.AddMember("task_id", STRING_REF(resp->task_id), allocator);
        data.AddMember("user_name", STRING_REF(resp->user_name), allocator);
        data.AddMember("login_password", STRING_REF(resp->login_password), allocator);
        data.AddMember("ip", STRING_REF(resp->ip), allocator);
        data.AddMember("ssh_port", STRING_REF(resp->ssh_port), allocator);
        data.AddMember("create_time", STRING_REF(resp->create_time), allocator);
        data.AddMember("system_storage", STRING_REF(resp->system_storage), allocator);
        data.AddMember("data_storage", STRING_REF(resp->data_storage), allocator);
        data.AddMember("cpu_cores", STRING_REF(resp->cpu_cores), allocator);
        data.AddMember("gpu_count", STRING_REF(resp->gpu_count), allocator);
        data.AddMember("mem_size", STRING_REF(resp->mem_size), allocator);

        httpReq->reply_comm_rest_succ(data);

        return E_SUCCESS;
    }

    // start
    std::shared_ptr<dbc::network::message> rest_start_task(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path) {
        if (httpReq->get_request_method() != dbc::network::http_request::POST) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST,
                        "Only support POST requests. POST /api/v1/tasks/<task_id>/start")
            return nullptr;
        }

        std::vector<std::string> path_list;
        rest_util::split_path(path, path_list);

        if (path_list.size() != 2) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid api. Use /api/v1/tasks/<task_id>/start")
            return nullptr;
        }

        const std::string &task_id = path_list[0];

        std::shared_ptr<cmd_start_task_req> cmd_req = std::make_shared<cmd_start_task_req>();
        cmd_req->task_id = task_id;

        // parse body
        std::string body = httpReq->read_body();
        if (body.empty()) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid body. Use /api/v1/tasks/start")
            return nullptr;
        }

        rapidjson::Document doc;
        doc.Parse(body.c_str());
        if (!doc.IsObject()) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid JSON. Use /api/v1/tasks/start")
            return nullptr;
        }

        JSON_PARSE_STRING(doc, "additional", cmd_req->additional)
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
        msg->set_name(typeid(cmd_start_task_req).name());
        msg->set_content(cmd_req);
        return msg;
    }

    int32_t on_cmd_start_task_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg) {
        INIT_RSP_CONTEXT(cmd_start_task_req, cmd_start_task_rsp)

        if (resp->result != 0) {
            ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info)
            return E_DEFAULT;
        }

        SUCC_REPLY(data)
        return E_SUCCESS;
    }

    // stop
    std::shared_ptr<dbc::network::message> rest_stop_task(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path) {
        if (httpReq->get_request_method() != dbc::network::http_request::POST) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST,
                        "Only support POST requests. POST /api/v1/tasks/<task_id>/stop")
            return nullptr;
        }

        std::vector<std::string> path_list;
        rest_util::split_path(path, path_list);

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

    int32_t on_cmd_stop_task_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg) {
        INIT_RSP_CONTEXT(cmd_stop_task_req, cmd_stop_task_rsp)

        if (resp->result != 0) {
            ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info)
            return E_DEFAULT;
        }

        SUCC_REPLY(data)
        return E_SUCCESS;
    }

    // restart
    std::shared_ptr<dbc::network::message> rest_restart_task(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path) {
        if (httpReq->get_request_method() != dbc::network::http_request::POST) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST,
                        "Only support POST requests. POST /api/v1/tasks/<task_id>/start")
            return nullptr;
        }

        std::vector<std::string> path_list;
        rest_util::split_path(path, path_list);

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

    int32_t on_cmd_restart_task_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg) {
        INIT_RSP_CONTEXT(cmd_restart_task_req, cmd_restart_task_rsp)

        if (resp->result != 0) {
            ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info)
            return E_DEFAULT;
        }

        SUCC_REPLY(data)
        return E_SUCCESS;
    }

    // reset
    std::shared_ptr<dbc::network::message> rest_reset_task(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path) {
        if (httpReq->get_request_method() != dbc::network::http_request::POST) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST,
                        "Only support POST requests. POST /api/v1/tasks/<task_id>/reset")
            return nullptr;
        }

        std::vector<std::string> path_list;
        rest_util::split_path(path, path_list);

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

    int32_t on_cmd_reset_task_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg) {
        INIT_RSP_CONTEXT(cmd_reset_task_req, cmd_reset_task_rsp)

        if (resp->result != 0) {
            ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info)
            return E_DEFAULT;
        }

        SUCC_REPLY(data)
        return E_SUCCESS;
    }

    // delete
    std::shared_ptr<dbc::network::message> rest_delete_task(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path) {
        if (httpReq->get_request_method() != dbc::network::http_request::POST) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST,
                        "Only support POST requests. POST /api/v1/tasks/<task_id>/stop")
            return nullptr;
        }

        std::vector<std::string> path_list;
        rest_util::split_path(path, path_list);

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

    int32_t on_cmd_delete_task_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg) {
        INIT_RSP_CONTEXT(cmd_delete_task_req, cmd_delete_task_rsp)

        if (resp->result != 0) {
            ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info)
            return E_DEFAULT;
        }

        SUCC_REPLY(data)
        return E_SUCCESS;
    }

    // task logs
    std::shared_ptr<dbc::network::message> rest_task_logs(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path) {
        if (httpReq->get_request_method() != dbc::network::http_request::POST) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST,
                        "Only support POST requests. POST /api/v1/tasks/<task_id>/logs")
            return nullptr;
        }

        std::vector<std::string> path_list;
        rest_util::split_path(path, path_list);
        if (path_list.size() != 2) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS,
                        "Invalid path_list. GET /api/v1/tasks/{task_id}/trace?flag=tail&line_num=100")
            return nullptr;
        }

        const std::string &task_id = path_list[0];

        std::map<std::string, std::string> query_table;
        rest_util::split_path_into_kvs(path, query_table);

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

    int32_t on_cmd_task_logs_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg) {
        INIT_RSP_CONTEXT(cmd_task_logs_req, cmd_task_logs_rsp)

        if (resp->result != 0) {
            ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info)
            return E_DEFAULT;
        }

        data.AddMember("log_content", STRING_REF(resp->log_content), allocator);

        httpReq->reply_comm_rest_succ(data);

        return E_SUCCESS;
    }

    // list tasks
    std::shared_ptr<dbc::network::message> rest_list_task(const dbc::network::HTTP_REQUEST_PTR &httpReq, const std::string &path) {
        if (httpReq->get_request_method() != dbc::network::http_request::POST) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST,
                        "Only support POST requests. POST /api/v1/tasks")
            return nullptr;
        }

        std::shared_ptr<cmd_list_task_req> cmd_req = std::make_shared<cmd_list_task_req>();

        std::vector<std::string> path_list;
        rest_util::split_path(path, path_list);

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

    int32_t on_cmd_list_task_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg) {
        INIT_RSP_CONTEXT(cmd_list_task_req, cmd_list_task_rsp)

        if (resp->result != 0) {
            ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info)
            return 0;
        }

        if (req->task_id == "") {
            rapidjson::Value task_info_list(rapidjson::kArrayType);
            for (auto it = resp->task_info_list.begin(); it != resp->task_info_list.end(); it++) {
                rapidjson::Value st(rapidjson::kObjectType);
                st.AddMember("task_id", STRING_DUP(it->task_id), allocator);
                st.AddMember("login_password", STRING_DUP(it->pwd), allocator);
                st.AddMember("status", STRING_DUP(dbc::task_status_string(it->status)), allocator);

                task_info_list.PushBack(st, allocator);
            }
            data.AddMember("task_info_list", task_info_list, allocator);
            SUCC_REPLY(data)
            return E_SUCCESS;
        } else {
            rapidjson::Document document;
            rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

            if (resp->task_info_list.size() >= 1) {
                auto it = resp->task_info_list.begin();
                data.AddMember("task_id", STRING_REF(it->task_id), allocator);
                data.AddMember("login_password", STRING_DUP(it->pwd), allocator);
                data.AddMember("status", STRING_DUP(dbc::task_status_string(it->status)), allocator);
            }

            SUCC_REPLY(data)
            return E_SUCCESS;
        }
    }

    // modify task
    std::shared_ptr<dbc::network::message> rest_modify_task(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path) {
        if (httpReq->get_request_method() != dbc::network::http_request::POST) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST,
                        "Only support POST requests. POST /api/v1/tasks/<task_id>/stop")
            return nullptr;
        }

        std::vector<std::string> path_list;
        rest_util::split_path(path, path_list);

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

    int32_t on_cmd_modify_task_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg) {
        INIT_RSP_CONTEXT(cmd_modify_task_req, cmd_modify_task_rsp)

        if (resp->result != 0) {
            ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info)
            return E_DEFAULT;
        }

        SUCC_REPLY(data)
        return E_SUCCESS;
    }

    // mining_nodes/
    std::shared_ptr<dbc::network::message> rest_mining_nodes(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path) {
        if (httpReq->get_request_method() != dbc::network::http_request::POST) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "Only support POST requests")
            return nullptr;
        }

        std::vector<std::string> path_list;
        rest_util::split_path(path, path_list);

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
        std::map<std::string, node_service_info> s_in_order;
        for (auto &it : *(resp->id_2_services)) {
            s_in_order[it.first] = it.second;
        }

        rapidjson::Value mining_nodes(rapidjson::kArrayType);
        for (auto &it : s_in_order) {
            rapidjson::Value node_info(rapidjson::kObjectType);
            std::string node_id = it.first;
            node_id = string_util::rtrim(node_id, '\n');
            std::string ver = it.second.kvs.count("version") ? it.second.kvs["version"] : "N/A";

            std::string service_list = resp->to_string(it.second.service_list);
            node_info.AddMember("service_list", STRING_DUP(service_list), allocator);
            node_info.AddMember("nodeid", STRING_DUP(node_id), allocator);
            node_info.AddMember("name", STRING_DUP(it.second.name), allocator);
            node_info.AddMember("ver", STRING_DUP(ver), allocator);
            node_info.AddMember("state", STRING_DUP(it.second.kvs["state"]), allocator);

            mining_nodes.PushBack(node_info, allocator);
        }

        data.AddMember("mining_nodes", mining_nodes, allocator);
    }

    int32_t on_list_node_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg) {
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

    // /peers/
    std::shared_ptr<dbc::network::message> rest_peers(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path) {
        std::vector<std::string> path_list;
        rest_util::split_path(path, path_list);

        if (path_list.size() != 1) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS,
                        "No option count specified. Use /api/v1/peers/{option}")
            return nullptr;
        }

        const std::string &option = path_list[0];
        std::shared_ptr<cmd_get_peer_nodes_req> req = std::make_shared<cmd_get_peer_nodes_req>();
        req->flag = matrix::service_core::flag_active;

        if (option == "active") {
            req->flag = matrix::service_core::flag_active;
        } else if (option == "global") {
            req->flag = matrix::service_core::flag_global;
        } else {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid option. The option is active or global.")
            return nullptr;
        }

        std::shared_ptr<dbc::network::message> msg = std::make_shared<dbc::network::message>();
        msg->set_name(typeid(cmd_get_peer_nodes_req).name());
        msg->set_content(req);
        return msg;
    }

    int32_t on_cmd_get_peer_nodes_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg) {
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
            cmd_peer_node_info.AddMember("ip", STRING_DUP(string_util::fuzz_ip(it->addr.ip)), allocator);
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
    std::shared_ptr<dbc::network::message> rest_stat(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path) {
        rapidjson::Document document;
        rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

        rapidjson::Value data(rapidjson::kObjectType);
        std::string node_id = CONF_MANAGER->get_node_id();

        shared_ptr<module_manager> mdls = g_server->get_module_manager();
        dbc::rest_api_service *s = (dbc::rest_api_service *) mdls->get(REST_API_SERVICE_MODULE).get();

        data.AddMember("node_id", STRING_REF(node_id), allocator);
        data.AddMember("session_count", s->get_session_count(), allocator);
        data.AddMember("startup_time", s->get_startup_time(), allocator);
        SUCC_REPLY(data)
        return nullptr;
    }

    // /config/
    std::shared_ptr<dbc::network::message> rest_config(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path) {
        std::vector<std::string> path_list;
        rest_util::split_path(path, path_list);

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
    std::shared_ptr<dbc::network::message> rest_api_version(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path) {
        rapidjson::Document document;
        rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

        rapidjson::Value data(rapidjson::kObjectType);

        data.AddMember("version", STRING_REF(REST_API_VERSION), allocator);

        std::string node_id = CONF_MANAGER->get_node_id();

        data.AddMember("node_id", STRING_REF(node_id), allocator);
        SUCC_REPLY(data)
        return nullptr;
    }
}
