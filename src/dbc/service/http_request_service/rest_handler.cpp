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

/*
 * Unified successful response method
 * */
#define ERROR_REPLY(status, error_code, error_message) httpReq->reply_comm_rest_err(status,error_code,error_message);

/*
 * Unified error response method
 * */
#define SUCC_REPLY(data) httpReq->reply_comm_rest_succ(data);

/*
 * Insert a spefic value in to  bpo::variables_map vm;
 * */
#define INSERT_VARIABLE(vm, k) variable_value k##_;k##_.value() = k;vm.insert({ #k, k##_ });

/*
 * Initialize the context information that handles a response
 * */
#define INIT_RSP_CONTEXT(CMD_REQ, CMD_RSP) \
std::shared_ptr<http_request>& httpReq=hreq_context->m_hreq;\
std::shared_ptr<message>& req_msg=hreq_context->m_req_msg;\
std::shared_ptr<CMD_REQ> req = std::dynamic_pointer_cast<CMD_REQ>(req_msg->content);\
std::shared_ptr<CMD_RSP> resp = std::dynamic_pointer_cast<CMD_RSP>(resp_msg->content);\
rapidjson::Document document;\
rapidjson::Document::AllocatorType& allocator = document.GetAllocator();\
rapidjson::Value data(rapidjson::kObjectType);

#define RETURN_REQ_MSG(cmd)\
std::shared_ptr<message> msg = std::make_shared<message>();\
msg->set_name(typeid(cmd).name());\
msg->set_content(req);\
return msg;

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
    std::shared_ptr<message> rest_task(const HTTP_REQUEST_PTR &httpReq, const std::string &path) {
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

            // /tasks/<task_id>/result
            if (second_param == "logs") {
                return rest_task_logs(httpReq, path);
            }
        }

        ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "invalid requests uri")
        return nullptr;
    }

    // create
    std::shared_ptr<message> rest_create_task(const HTTP_REQUEST_PTR &httpReq, const std::string &path) {
        if (httpReq->get_request_method() != http_request::POST) {
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

        std::shared_ptr<message> msg = std::make_shared<message>();
        msg->set_name(typeid(cmd_create_task_req).name());
        msg->set_content(cmd_req);
        return msg;
    }

    int32_t on_cmd_create_task_rsp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg) {
        INIT_RSP_CONTEXT(cmd_create_task_req, cmd_create_task_rsp)

        if (resp->result != 0) {
            ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info)
            return E_DEFAULT;
        }

        data.AddMember("task_id", STRING_REF(resp->task_id), allocator);
        data.AddMember("password", STRING_REF(resp->login_password), allocator);

        httpReq->reply_comm_rest_succ(data);

        return E_SUCCESS;
    }

    // start
    std::shared_ptr<message> rest_start_task(const HTTP_REQUEST_PTR& httpReq, const std::string &path) {
        if (httpReq->get_request_method() != http_request::POST) {
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

        std::shared_ptr<message> msg = std::make_shared<message>();
        msg->set_name(typeid(cmd_start_task_req).name());
        msg->set_content(cmd_req);
        return msg;
    }

    int32_t on_cmd_start_task_rsp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg) {
        INIT_RSP_CONTEXT(cmd_start_task_req, cmd_start_task_rsp)

        if (resp->result != 0) {
            ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info)
            return E_DEFAULT;
        }

        SUCC_REPLY(data)
        return E_SUCCESS;
    }

    // restart
    std::shared_ptr<message> rest_restart_task(const HTTP_REQUEST_PTR& httpReq, const std::string &path) {
        if (httpReq->get_request_method() != http_request::POST) {
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

        std::shared_ptr<message> msg = std::make_shared<message>();
        msg->set_name(typeid(cmd_restart_task_req).name());
        msg->set_content(cmd_req);
        return msg;
    }

    int32_t on_cmd_restart_task_rsp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg) {
        INIT_RSP_CONTEXT(cmd_restart_task_req, cmd_restart_task_rsp)

        if (resp->result != 0) {
            ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info)
            return E_DEFAULT;
        }

        SUCC_REPLY(data)
        return E_SUCCESS;
    }

    // stop
    std::shared_ptr<message> rest_stop_task(const HTTP_REQUEST_PTR& httpReq, const std::string &path) {
        if (httpReq->get_request_method() != http_request::POST) {
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

        std::shared_ptr<message> msg = std::make_shared<message>();
        msg->set_name(typeid(cmd_stop_task_req).name());
        msg->set_content(cmd_req);
        return msg;
    }

    int32_t on_cmd_stop_task_rsp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg) {
        INIT_RSP_CONTEXT(cmd_stop_task_req, cmd_stop_task_rsp)

        if (resp->result != 0) {
            ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info)
            return E_DEFAULT;
        }

        SUCC_REPLY(data)
        return E_SUCCESS;
    }

    // list tasks
    std::shared_ptr<message> rest_list_task(const HTTP_REQUEST_PTR &httpReq, const std::string &path) {
        if (httpReq->get_request_method() != http_request::POST) {
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

        std::shared_ptr<message> msg = std::make_shared<message>();
        msg->set_name(typeid(cmd_list_task_req).name());
        msg->set_content(cmd_req);

        return msg;
    }

    int32_t on_cmd_list_task_rsp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg) {
        INIT_RSP_CONTEXT(cmd_list_task_req, cmd_list_task_rsp)

        if (resp->result != 0) {
            ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info)
            return 0;
        }

        if (req->task_id == "") {
            rapidjson::Value task_status_list(rapidjson::kArrayType);
            for (auto it = resp->task_status_list.begin(); it != resp->task_status_list.end(); it++) {
                rapidjson::Value st(rapidjson::kObjectType);
                st.AddMember("task_id", STRING_DUP(it->task_id), allocator);
                st.AddMember("login_password", STRING_DUP(it->pwd), allocator);
                st.AddMember("status", STRING_DUP(dbc::task_status_string(it->status)), allocator);

                task_status_list.PushBack(st, allocator);
            }
            data.AddMember("task_status_list", task_status_list, allocator);
            SUCC_REPLY(data)
            return E_SUCCESS;
        } else {
            rapidjson::Document document;
            rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

            if (resp->task_status_list.size() >= 1) {
                auto it = resp->task_status_list.begin();
                data.AddMember("task_id", STRING_REF(it->task_id), allocator);
                data.AddMember("login_password", STRING_DUP(it->pwd), allocator);
                data.AddMember("status", STRING_DUP(dbc::task_status_string(it->status)), allocator);
            }

            SUCC_REPLY(data)
            return E_SUCCESS;
        }
    }

    // logs
    std::shared_ptr<message> rest_task_logs(const HTTP_REQUEST_PTR& httpReq, const std::string &path) {
        if (httpReq->get_request_method() != http_request::POST) {
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

        std::shared_ptr<message> msg = std::make_shared<message>();
        msg->set_name(typeid(cmd_task_logs_req).name());
        msg->set_content(cmd_req);
        return msg;
    }

    int32_t on_cmd_task_logs_rsp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg) {
        INIT_RSP_CONTEXT(cmd_task_logs_req, cmd_task_logs_rsp)

        if (resp->result != 0) {
            ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info)
            return E_DEFAULT;
        }

        rapidjson::Value peer_node_logs(rapidjson::kArrayType);
        for (auto s = resp->peer_node_logs.begin(); s != resp->peer_node_logs.end(); s++) {
            rapidjson::Value log(rapidjson::kObjectType);
            log.AddMember("peer_node_id", STRING_REF(s->peer_node_id), allocator);
            log.AddMember("log_content", STRING_REF(s->log_content), allocator);

            peer_node_logs.PushBack(log, allocator);
        }

        data.AddMember("peer_node_logs", peer_node_logs, allocator);

        httpReq->reply_comm_rest_succ(data);

        return E_SUCCESS;
    }

    // /mining_nodes/
    std::shared_ptr<message> rest_mining_nodes(const HTTP_REQUEST_PTR& httpReq, const std::string &path) {
        std::vector<std::string> path_list;
        rest_util::split_path(path, path_list);

        if (path_list.size() > 2) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS,
                        "No nodeid count specified. Use /api/v1/mining_nodes/{nodeid}/{key}")
            return nullptr;
        }

        std::string nodeid;
        if (!path_list.empty()) {
            nodeid = path_list[0];
        }

        std::shared_ptr<cmd_list_node_req> req = std::make_shared<cmd_list_node_req>();
        req->op = OP_SHOW_UNKNOWN;

        if (!nodeid.empty()) {
            req->o_node_id = CONF_MANAGER->get_node_id();
            req->d_node_id = nodeid;
            req->op = OP_SHOW_NODE_INFO;

            std::string key;
            if (path_list.size() >= 2) {
                key = path_list[1];
            }

            if (key.empty()) {
                req->keys.emplace_back("all");
            } else {
                req->keys.push_back(key);
            }
        } else {
            req->op = OP_SHOW_SERVICE_LIST;
        }

        std::shared_ptr<message> msg = std::make_shared<message>();
        msg->set_name(typeid(cmd_list_node_req).name());
        msg->set_content(req);
        return msg;
    }

    void reply_show_nodeinfo(const std::shared_ptr<cmd_list_node_rsp> &resp, rapidjson::Value &data,
                             rapidjson::Document::AllocatorType &allocator) {
        for (auto &kv: resp->kvs) {
            if (kv.second.length() > 0 &&
                (kv.second[0] == '{' || kv.second[0] == '[')) {
                rapidjson::Document doc;

                if (!doc.Parse<0>(kv.second.c_str()).HasParseError()) {
                    rapidjson::Value v = rapidjson::Value(doc, allocator);

                    data.AddMember(STRING_DUP(kv.first), v, allocator);
                }
            } else {
                // legacy gpu state or  gpu usage
                if (kv.first == std::string("image")) {
                    rapidjson::Value images(rapidjson::kArrayType);
                    std::vector<std::string> image_list;
                    string_util::split(resp->kvs["image"], "\n", image_list);
                    for (auto &img : image_list) {
                        images.PushBack(STRING_DUP(img), allocator);
                    }

                    data.AddMember("images", images, allocator);
                } else if (kv.first == std::string("gpu_usage")) {
                    //   gpu_usage_str:
                    //"gpu: 0 %\nmem: 0 %\ngpu: 0 %\nmem: 0 %\ngpu: 0 %\nmem: 0 %\ngpu: 0 %\nmem: 0 %\n"
                    //
                    std::string gpu_usage_str = string_util::rtrim(resp->kvs["gpu_usage"], '\n');
                    rapidjson::Value gpu_usage(rapidjson::kArrayType);
                    std::vector<std::string> gpu_usage_list;
                    string_util::split(gpu_usage_str, "\n", gpu_usage_list);

                    if (gpu_usage_list.size() % 2 == 0) {
                        for (auto i = 0; i < gpu_usage_list.size(); i += 2) {
                            std::string gpu;
                            rest_util::get_value_from_string(gpu_usage_list[i], "gpu", gpu);

                            std::string mem;
                            rest_util::get_value_from_string(gpu_usage_list[i + 1], "mem", mem);

                            rapidjson::Value g(rapidjson::kObjectType);
                            g.AddMember("gpu", STRING_DUP(gpu), allocator);
                            g.AddMember("mem", STRING_DUP(mem), allocator);

                            gpu_usage.PushBack(g, allocator);

                        }
                    }

                    data.AddMember("gpu_usage", gpu_usage, allocator);

                } else {
                    fill_json_filed_string(data, allocator, kv.first, kv.second);
                }
            }
        }

#if 0

        std::string cpu_info = resp->kvs["cpu"];
        string_util::trim(cpu_info);
        int pos = cpu_info.find_first_of(' ');
        if (pos!=std::string::npos) {
            std::string cpu_num_str = cpu_info.substr(0, pos);
            string_util::trim(cpu_num_str);
            if (!cpu_num_str.empty()) {
                int cpu_num = atoi(cpu_num_str.c_str());
                data.AddMember("cpu_num", cpu_num, allocator);
            } else {
                data.AddMember("cpu_num", -1, allocator);
            }
        } else {
            data.AddMember("cpu_num", -1, allocator);
        }

        fill_json_filed_string(data, allocator, "cpu", cpu_info);
        fill_json_filed_string(data, allocator, "cpu_usage", resp->kvs["cpu_usage"]);

        fill_json_filed_string(data, allocator, "gpu", resp->kvs["gpu"]);
        fill_json_filed_string(data, allocator, "gpu_driver", resp->kvs["gpu_driver"]);

        fill_json_filed_string(data, allocator, "mem_usage", resp->kvs["mem_usage"]);
        fill_json_filed_string(data, allocator, "startup_time", resp->kvs["startup_time"]);
        fill_json_filed_string(data, allocator, "state", resp->kvs["state"]);
        fill_json_filed_string(data, allocator, "version", resp->kvs["version"]);

        //   gpu_usage_str:
        //"gpu: 0 %\nmem: 0 %\ngpu: 0 %\nmem: 0 %\ngpu: 0 %\nmem: 0 %\ngpu: 0 %\nmem: 0 %\n"
        //
        std::string gpu_usage_str = string_util::rtrim(resp->kvs["gpu_usage"], '\n');
        rapidjson::Value gpu_usage(rapidjson::kArrayType);
        std::vector<std::string> gpu_usage_list;
        string_util::split(gpu_usage_str, "\n", gpu_usage_list);

        if (gpu_usage_list.size()%2==0) {
            for (auto i = 0; i < gpu_usage_list.size(); i += 2) {
                std::string gpu;
                rest_util::get_value_from_string(gpu_usage_list[i], "gpu", gpu);

                std::string mem;
                rest_util::get_value_from_string(gpu_usage_list[i + 1], "mem", mem);

                rapidjson::Value g(rapidjson::kObjectType);
                g.AddMember("gpu", STRING_DUP(gpu), allocator);
                g.AddMember("mem", STRING_DUP(mem), allocator);

                gpu_usage.PushBack(g, allocator);

            }
        }

        data.AddMember("gpu_usage", gpu_usage, allocator);


        /*
         Text:

         Filesystem                 Size  Used Avail Use% Mounted on
        /dev/mapper/8GPU--vg-root   57G  5.4G   48G  11% /
        /dev/sda1                  917G   52G  819G   6% /dbcdata
        /dev/sdi2                  473M  232M  218M  52% /boot
        /dev/sdi1                  511M  6.1M  505M   2% /boot/efi

         JSON:

         {
                        "fs": "/dev/bcache0",
                        "size": "224 G",
                        "used": "81G",
                        "avail": "132G",
                        "use_percent": "38%",
                        "mounted_on": "/data"
         }
         * */
        static constexpr uint32_t DEFAULT_DISK_ITEM_SIZE = 6;
        rapidjson::Value disks(rapidjson::kArrayType);
        std::string disk_str = resp->kvs["disk"];
        std::vector<std::string> disk_line_list;
        string_util::split(disk_str, "\n", disk_line_list);

        for (auto& disk_line : disk_line_list) {
            std::vector<std::string> item_list;
            rest_util::split_line_to_itemlist(disk_line, item_list);
            if (item_list.size()==DEFAULT_DISK_ITEM_SIZE) {
                rapidjson::Value d(rapidjson::kObjectType);
                d.AddMember("fs", STRING_DUP(item_list[0]), allocator);
                d.AddMember("size", STRING_DUP(item_list[1]), allocator);
                d.AddMember("used", STRING_DUP(item_list[2]), allocator);
                d.AddMember("avail", STRING_DUP(item_list[3]), allocator);
                d.AddMember("use_percent", STRING_DUP(item_list[4]), allocator);
                d.AddMember("mounted_on", STRING_DUP(item_list[5]), allocator);
                disks.PushBack(d, allocator);
            }
        }
        data.AddMember("disks", disks, allocator);


        /*
          "images": [
              "dbctraining/cuda9.0-mining-gpu:v1",
              "dbctraining/tensorflow1.9.0-cuda9-gpu-py3:v1.0.0",

            ],
         */

        rapidjson::Value images(rapidjson::kArrayType);
        std::vector<std::string> image_list;
        string_util::split(resp->kvs["image"], "\n", image_list);
        for (auto& img : image_list) {
            images.PushBack(STRING_DUP(img), allocator);
        }

        data.AddMember("images", images, allocator);


        /*
        "mem": {
              "total": "251G",
              "free": "249G"
            }
         * */
        rapidjson::Value mem(rapidjson::kObjectType);
        std::string mem_total;
        rest_util::get_value_from_string(resp->kvs["mem"], "total", mem_total);

        std::string mem_free;
        rest_util::get_value_from_string(resp->kvs["mem"], "free", mem_free);

        mem.AddMember("total", STRING_DUP(mem_total), allocator);
        mem.AddMember("free", STRING_DUP(mem_free), allocator);

        data.AddMember("mem", mem, allocator);
#endif

    }

    void reply_show_service(const std::shared_ptr<cmd_list_node_rsp> &resp, rapidjson::Value &data,
                            rapidjson::Document::AllocatorType &allocator) {

        std::map<std::string, node_service_info> s_in_order;
        for (auto &it : *(resp->id_2_services)) {
            std::string k;
            k = it.second.name;// or time_stamp ,or ..

            auto v = it.second;
            v.kvs["id"] = it.first;
            s_in_order[k + it.first] = v;  // "k + id" is unique
        }

        rapidjson::Value mining_nodes(rapidjson::kArrayType);

        for (auto &it : s_in_order) {

            rapidjson::Value node_info(rapidjson::kObjectType);
            std::string nid = string_util::rtrim(it.second.kvs["id"], '\n');
            std::string ver = it.second.kvs.count("version") ? it.second.kvs["version"] : "N/A";
            std::string gpu = string_util::rtrim(it.second.kvs["gpu"], '\n');
            string_util::trim(gpu);
            if (gpu.length() > 31) {
                gpu = gpu.substr(0, 31);
            }

            std::string gpu_usage = it.second.kvs.count("gpu_usage") ? it.second.kvs["gpu_usage"] : "N/A";

            std::string online_time = "N/A";
            if (it.second.kvs.count("startup_time")) {
                std::string s_time = it.second.kvs["startup_time"];
                try {
                    time_t t = std::stoi(s_time);
                    online_time = resp->to_time_str(t);
                } catch (...) {
                    LOG_ERROR << "std::stoi(s_time);caught exception.";
                }
            }

            std::string service_list = resp->to_string(it.second.service_list);
            node_info.AddMember("service_list", STRING_DUP(service_list), allocator);

            node_info.AddMember("nodeid", STRING_DUP(nid), allocator);
            node_info.AddMember("name", STRING_DUP(it.second.name), allocator);
            node_info.AddMember("time_stamp", STRING_DUP(online_time), allocator);
            node_info.AddMember("gpu", STRING_DUP(gpu), allocator);
            node_info.AddMember("ver", STRING_DUP(ver), allocator);
            node_info.AddMember("gpu_usage", STRING_DUP(gpu_usage), allocator);
            node_info.AddMember("state", STRING_DUP(it.second.kvs["state"]), allocator);
            mining_nodes.PushBack(node_info, allocator);
        }

        data.AddMember("mining_nodes", mining_nodes, allocator);
    }

    int32_t on_list_node_rsp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg) {
        INIT_RSP_CONTEXT(cmd_list_node_req, cmd_list_node_rsp)

        if (!resp->err.empty()) {
            ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->err)
            return E_DEFAULT;
        }

        if (resp->op == OP_SHOW_SERVICE_LIST) {
            reply_show_service(resp, data, allocator);
        } else if (resp->op == OP_SHOW_NODE_INFO) {
            reply_show_nodeinfo(resp, data, allocator);
        }

        SUCC_REPLY(data)
        return E_SUCCESS;
    }

    // /peers/
    std::shared_ptr<message> rest_peers(const HTTP_REQUEST_PTR& httpReq, const std::string &path) {
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

        std::shared_ptr<message> msg = std::make_shared<message>();
        msg->set_name(typeid(cmd_get_peer_nodes_req).name());
        msg->set_content(req);
        return msg;
    }

    int32_t on_cmd_get_peer_nodes_rsp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg) {
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
    std::shared_ptr<message> rest_stat(const HTTP_REQUEST_PTR& httpReq, const std::string &path) {
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
    std::shared_ptr<message> rest_config(const HTTP_REQUEST_PTR& httpReq, const std::string &path) {
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
    std::shared_ptr<message> rest_api_version(const HTTP_REQUEST_PTR& httpReq, const std::string &path) {
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
