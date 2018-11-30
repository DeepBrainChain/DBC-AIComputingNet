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
#include "service_common_def.h"
#include "task_common_def.h"
#include "http_server_service.h"
#include "log.h"
#include "json_util.h"
#include "rpc_error.h"
#include "api_call_handler.h"
#include <vector>
#include <map>
#include <boost/format.hpp>
#include <random.h>

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
#define INIT_RSP_CONTEXT(CMD_REQ, CMD_RSP) std::shared_ptr<http_request>& httpReq=hreq_context->m_hreq;\
std::shared_ptr<message>& req_msg=hreq_context->m_req_msg;\
std::shared_ptr<CMD_REQ> req = std::dynamic_pointer_cast<CMD_REQ>(req_msg->content);\
std::shared_ptr<CMD_RSP> resp = std::dynamic_pointer_cast<CMD_RSP>(resp_msg->content);\
rapidjson::Document document;\
rapidjson::Document::AllocatorType& allocator = document.GetAllocator();\
rapidjson::Value data(rapidjson::kObjectType);\
\


/*
 * Return msg from req.
 * */
#define RETURN_REQ_MSG(cmd)\
std::shared_ptr<message> msg = std::make_shared<message>();\
msg->set_name(typeid(cmd).name());\
msg->set_content(req);\
return msg;\
\


namespace ai
{
    namespace dbc
    {

        //
        // Populate the formatted string into the json structure
        //

        inline void fill_json_filed_string(rapidjson::Value& data,
                                           rapidjson::Document::AllocatorType& allocator,
                                           const std::string key,
                                           const std::string& val)
        {
            if (val.empty()) {
                return;
            }
            std::string tmp_val(val);
            tmp_val = string_util::rtrim(tmp_val, '\n');
            string_util::trim(tmp_val);

            data.AddMember(STRING_DUP(key), STRING_DUP(tmp_val), allocator);

        }

        int32_t on_get_peer_nodes_resp(HTTP_REQ_CTX_PTR hreq_context, std::shared_ptr<message>& resp_msg)
        {
            INIT_RSP_CONTEXT(cmd_get_peer_nodes_req, cmd_get_peer_nodes_resp);

            if (resp->result!=0) {
                ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info);
                return E_DEFAULT;
            }

            rapidjson::Value peer_nodes_list(rapidjson::kArrayType);
            for (auto it = resp->peer_nodes_list.begin(); it!=resp->peer_nodes_list.end(); it++) {
                rapidjson::Value cmd_peer_node_info(rapidjson::kObjectType);

                cmd_peer_node_info.AddMember("peer_node_id", STRING_REF(it->peer_node_id), allocator);
                cmd_peer_node_info.AddMember("live_time_stamp", it->live_time_stamp, allocator);

                cmd_peer_node_info.AddMember("net_st", STRING_DUP(net_state_2_string(it->net_st)), allocator);
                cmd_peer_node_info.AddMember("ip", STRING_DUP(string_util::fuzz_ip(it->addr.ip)), allocator);
                cmd_peer_node_info.AddMember("port", it->addr.port, allocator);
                cmd_peer_node_info.AddMember("node_type", STRING_DUP(node_type_2_string(it->node_type)), allocator);

                rapidjson::Value service_list(rapidjson::kArrayType);
                for (auto s = it->service_list.begin(); s!=it->service_list.end(); s++) {
                    service_list.PushBack(rapidjson::StringRef(s->c_str(), s->length()), allocator);
                }
                cmd_peer_node_info.AddMember("service_list", service_list, allocator);

                peer_nodes_list.PushBack(cmd_peer_node_info, allocator);
            }

            data.AddMember("peer_nodes_list", peer_nodes_list, allocator);

            SUCC_REPLY(data);
            return E_SUCCESS;
        }

        std::shared_ptr<message> rest_peers(HTTP_REQUEST_PTR httpReq, const std::string& path)
        {
            std::vector<std::string> path_list;
            rest_util::split_path(path, path_list);

            if (path_list.size()!=1) {
                ERROR_REPLY(HTTP_BADREQUEST,
                            RPC_INVALID_PARAMS,
                            "No option count specified. Use /api/v1/peers/{option}");
                return nullptr;
            }

            const std::string& option = path_list[0];
            std::shared_ptr<cmd_get_peer_nodes_req> req = std::make_shared<cmd_get_peer_nodes_req>();
            req->flag = matrix::service_core::flag_active;

            if (option=="active") {
                req->flag = matrix::service_core::flag_active;
            } else if (option=="global") {
                req->flag = matrix::service_core::flag_global;
            } else {
                ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid option. The option is active or global.");
                return nullptr;

            }

            RETURN_REQ_MSG(cmd_get_peer_nodes_req);
        }

        void reply_show_nodeinfo(const std::shared_ptr<cmd_show_resp>& resp,
                                 rapidjson::Value& data,
                                 rapidjson::Document::AllocatorType& allocator)
        {

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

        }

        void reply_show_service(const std::shared_ptr<cmd_show_resp>& resp,
                                rapidjson::Value& data,
                                rapidjson::Document::AllocatorType& allocator)
        {

            std::map<std::string, node_service_info> s_in_order;
            for (auto& it : *(resp->id_2_services)) {
                std::string k;
                k = it.second.name;// or time_stamp ,or ..

                auto v = it.second;
                v.kvs["id"] = it.first;
                s_in_order[k + it.first] = v;  // "k + id" is unique
            }

            rapidjson::Value mining_nodes(rapidjson::kArrayType);

            for (auto& it : s_in_order) {

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

        int32_t on_show_resp(HTTP_REQ_CTX_PTR hreq_context, std::shared_ptr<message>& resp_msg)
        {
            INIT_RSP_CONTEXT(cmd_show_req, cmd_show_resp);

            if (resp->err!="") {
                ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->err);
                return E_DEFAULT;
            }

            if (resp->op==OP_SHOW_SERVICE_LIST) {
                reply_show_service(resp, data, allocator);
            } else if (resp->op==OP_SHOW_NODE_INFO) {
                reply_show_nodeinfo(resp, data, allocator);
            }

            SUCC_REPLY(data);
            return E_SUCCESS;
        }

        std::shared_ptr<message> rest_mining_nodes(HTTP_REQUEST_PTR httpReq, const std::string& path)
        {
            std::vector<std::string> path_list;
            rest_util::split_path(path, path_list);

            if (path_list.size() > 1) {
                ERROR_REPLY(HTTP_BADREQUEST,
                            RPC_INVALID_PARAMS,
                            "No nodeid count specified. Use /api/v1/mining_nodes/{nodeid}");
                return nullptr;
            }

            std::string nodeid;
            if (path_list.size() >= 1) {
                nodeid = path_list[0];
            }

            std::shared_ptr<cmd_show_req> req = std::make_shared<cmd_show_req>();
            req->op = OP_SHOW_UNKNOWN;

            if (!nodeid.empty()) {
                req->o_node_id = CONF_MANAGER->get_node_id();
                req->d_node_id = nodeid;
                req->op = OP_SHOW_NODE_INFO;
                req->keys.push_back("all");
            } else {
                req->op = OP_SHOW_SERVICE_LIST;
            }
            RETURN_REQ_MSG(cmd_show_req);

        }

        std::shared_ptr<message> rest_task(HTTP_REQUEST_PTR httpReq, const std::string& path)
        {
            std::vector<std::string> path_list;
            rest_util::split_path(path, path_list);

            if (path_list.size()==0) {
                return rest_task_list(httpReq, path);
            }

            if (path_list.size()==1) {

                const std::string& first_param = path_list[0];
                if (first_param=="start") {
                    return rest_task_start(httpReq, path);
                }

                return rest_task_info(httpReq, path);

            }

            if (path_list.size()==2) {
                const std::string& second_param = path_list[1];
                if (second_param=="stop") {
                    return rest_task_stop(httpReq, path);
                }

                if (second_param=="result") {
                    return rest_task_result(httpReq, path);
                }

                if (second_param=="trace") {
                    return rest_log(httpReq, path);
                }
            }

            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "Invalid requests. "
                                                              " GET /api/v1/tasks,"
                                                              "or POST /api/v1/tasks/start,"
                                                              "or POST /api/v1/tasks/<task_id>/stop,"
                                                              "or POST /api/v1/tasks/<task_id>/result,"
                                                              "or GET /api/v1/tasks/{task_id}/trace?flag=tail&line_num=100,"
                                                              "or GET /api/v1/tasks/{task_id}");
            return nullptr;
        }

        int32_t on_list_training_resp(HTTP_REQ_CTX_PTR hreq_context, std::shared_ptr<message>& resp_msg)
        {
            INIT_RSP_CONTEXT(cmd_list_training_req, cmd_list_training_resp);

            if (resp->result!=0) {
                ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info);
                return 0;
            }

            if (req->list_type==LIST_ALL_TASKS) {
                rapidjson::Value task_status_list(rapidjson::kArrayType);
                for (auto it = resp->task_status_list.begin(); it!=resp->task_status_list.end(); it++) {
                    rapidjson::Value st(rapidjson::kObjectType);

                    st.AddMember("task_id", STRING_DUP(it->task_id), allocator);
                    st.AddMember("create_time", (int64_t) it->create_time, allocator);
                    st.AddMember("status", STRING_DUP(to_training_task_status_string(it->status)), allocator);

                    task_status_list.PushBack(st, allocator);
                }
                data.AddMember("task_status_list", task_status_list, allocator);
                SUCC_REPLY(data);
                return E_SUCCESS;
            } else {

                if (resp->task_status_list.size()!=1) {
                    ERROR_REPLY(HTTP_OK, RPC_RESPONSE_TIMEOUT, "The task is not found");
                    return E_DEFAULT;
                }

                rapidjson::Document document;
                rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

                auto it = resp->task_status_list.begin();

                data.AddMember("task_id", STRING_REF(it->task_id), allocator);
                data.AddMember("create_time", (int64_t) it->create_time, allocator);
                data.AddMember("status", STRING_DUP(to_training_task_status_string(it->status)), allocator);

                SUCC_REPLY(data);
                return E_SUCCESS;

            }

        }

        /*
         * GET /api/v1/tasks

         * */
        std::shared_ptr<message> rest_task_list(HTTP_REQUEST_PTR httpReq, const std::string& path)
        {
            std::vector<std::string> path_list;
            rest_util::split_path(path, path_list);

            if (path_list.size()!=0) {
                ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid path_list. GET /api/v1/tasks");
                return nullptr;
            }

            std::shared_ptr<cmd_list_training_req> req = std::make_shared<cmd_list_training_req>();
            req->list_type = LIST_ALL_TASKS;

            RETURN_REQ_MSG(cmd_list_training_req);

        }

        int32_t on_start_training_resp(HTTP_REQ_CTX_PTR hreq_context, std::shared_ptr<message>& resp_msg)
        {

            INIT_RSP_CONTEXT(cmd_start_training_req, cmd_start_training_resp);

            if (resp->result!=0) {
                ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info);
                return E_DEFAULT;
            }

            cmd_task_info& task_info = resp->task_info;

            data.AddMember("task_id", STRING_REF(task_info.task_id), allocator);
            data.AddMember("create_time", task_info.create_time, allocator);
            data.AddMember("status", STRING_DUP(to_training_task_status_string(task_info.status)), allocator);

            SUCC_REPLY(data);
            return E_SUCCESS;

        }

        std::shared_ptr<message> rest_task_start(HTTP_REQUEST_PTR httpReq, const std::string& path)
        {
            if (httpReq->get_request_method()!=http_request::POST) {
                ERROR_REPLY(HTTP_BADREQUEST,
                            RPC_INVALID_REQUEST,
                            "Only support POST requests. POST /api/v1/tasks/start");
                return nullptr;

            }

            std::vector<std::string> path_list;
            rest_util::split_path(path, path_list);

            if (path_list.size()!=1) {
                ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid api. Use /api/v1/tasks/start");
                return nullptr;
            }

            std::string body = httpReq->read_body();
            if (body.empty()) {
                ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid body. Use /api/v1/tasks/start");
                return nullptr;
            }

            rapidjson::Document document;
            try {
                document.Parse(body.c_str());
                if (!document.IsObject()) {
                    ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid JSON. Use /api/v1/tasks/start");
                    return nullptr;
                }
            } catch (...) {
                ERROR_REPLY(HTTP_BADREQUEST, RPC_MISC_ERROR, "Parse JSON Error. Use /api/v1/tasks/start");
                return nullptr;
            }

            std::string data_dir;
            std::string hyper_parameters;
            std::string entry_file;
            std::string code_dir;
            std::string training_engine;
            std::string master;
            std::string checkpoint_dir;
            std::string server_specification;
            std::string container_name;
            int32_t server_count = 0;
            int8_t select_mode = 0;
            std::vector<std::string> peer_nodes_list;

            JSON_PARSE_STRING(document, "code_dir", code_dir);
            JSON_PARSE_STRING(document, "data_dir", data_dir);
            JSON_PARSE_STRING(document, "hyper_parameters", hyper_parameters);
            JSON_PARSE_STRING(document, "entry_file", entry_file);
            JSON_PARSE_STRING(document, "training_engine", training_engine);
            JSON_PARSE_STRING(document, "master", master);
            JSON_PARSE_STRING(document, "checkpoint_dir", checkpoint_dir);
            JSON_PARSE_STRING(document, "server_specification", server_specification);
            JSON_PARSE_STRING(document, "container_name", container_name);
            JSON_PARSE_UINT(document, "select_mode", select_mode);
            JSON_PARSE_UINT(document, "server_count", server_count);

            if (document.HasMember("peer_nodes_list")) {
                if (document["peer_nodes_list"].IsArray()) {
                    for (rapidjson::SizeType i = 0; i < document["peer_nodes_list"].Size(); i++) {
                        std::string node(document["peer_nodes_list"][i].GetString());
                        peer_nodes_list.push_back(node);
                    }
                }
            }

            std::shared_ptr<cmd_start_training_req> req = std::make_shared<cmd_start_training_req>();
            bpo::variables_map& vm = req->vm;

            INSERT_VARIABLE(vm, peer_nodes_list);
            INSERT_VARIABLE(vm, code_dir);
            INSERT_VARIABLE(vm, data_dir);
            INSERT_VARIABLE(vm, hyper_parameters);
            INSERT_VARIABLE(vm, entry_file);
            INSERT_VARIABLE(vm, training_engine);
            INSERT_VARIABLE(vm, master);
            INSERT_VARIABLE(vm, checkpoint_dir);
            INSERT_VARIABLE(vm, server_specification);
            INSERT_VARIABLE(vm, container_name);
            INSERT_VARIABLE(vm, select_mode);
            INSERT_VARIABLE(vm, server_count);

            bpo::notify(vm);

            RETURN_REQ_MSG(cmd_start_training_req);
        }

        std::shared_ptr<message> rest_task_info(HTTP_REQUEST_PTR httpReq, const std::string& path)
        {
            std::vector<std::string> path_list;
            rest_util::split_path(path, path_list);

            if (path_list.size()!=1) {
                ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid path_list. GET /api/v1/tasks/{task_id}");
                return nullptr;
            }

            const std::string task_id = path_list[0];

            std::shared_ptr<cmd_list_training_req> req = std::make_shared<cmd_list_training_req>();

            req->list_type = LIST_SPECIFIC_TASKS;
            std::vector<std::string> task_vector;
            task_vector.push_back(task_id);

            std::copy(std::make_move_iterator(task_vector.begin()),
                      std::make_move_iterator(task_vector.end()),
                      std::back_inserter(req->task_list));

            RETURN_REQ_MSG(cmd_list_training_req);
        }

        int32_t on_stop_training_resp(HTTP_REQ_CTX_PTR hreq_context, std::shared_ptr<message>& resp_msg)
        {

            INIT_RSP_CONTEXT(cmd_stop_training_req, cmd_stop_training_resp);

            if (resp->result!=0) {
                ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info);
                return E_DEFAULT;
            }

            SUCC_REPLY(data);
            return E_SUCCESS;
        }

        std::shared_ptr<message> rest_task_stop(HTTP_REQUEST_PTR httpReq, const std::string& path)
        {
            std::vector<std::string> path_list;
            rest_util::split_path(path, path_list);

            if (path_list.size()!=2) {
                ERROR_REPLY(HTTP_BADREQUEST,
                            RPC_INVALID_PARAMS,
                            "Invalid path_list. POST /api/v1/tasks/<task_id>/stop");
                return nullptr;
            }

            const std::string& task_id = path_list[0];

            std::shared_ptr<cmd_stop_training_req> req = std::make_shared<cmd_stop_training_req>();
            req->task_id = task_id;

            RETURN_REQ_MSG(cmd_stop_training_req);
        }

        std::shared_ptr<message> rest_task_result(HTTP_REQUEST_PTR httpReq, const std::string& path)
        {
            std::vector<std::string> path_list;
            rest_util::split_path(path, path_list);

            if (path_list.size()!=2) {
                ERROR_REPLY(HTTP_BADREQUEST,
                            RPC_INVALID_PARAMS,
                            "Invalid path_list. GET /api/v1/tasks/<task_id>/result");
                return nullptr;
            }

            const std::string& task_id = path_list[0];

            std::shared_ptr<cmd_logs_req> req = std::make_shared<cmd_logs_req>();
            req->task_id = task_id;

            req->head_or_tail = GET_LOG_TAIL;

            req->number_of_lines = 100;

            req->sub_op = "result";

#if  defined(__linux__) || defined(MAC_OSX)
            req->dest_folder = "/tmp";
#elif defined(WIN32)
            req->dest_folder = "%TMP%";
#endif

            RETURN_REQ_MSG(cmd_logs_req);

        }

        int32_t on_logs_resp(HTTP_REQ_CTX_PTR hreq_context, std::shared_ptr<message>& resp_msg)
        {
            INIT_RSP_CONTEXT(cmd_logs_req, cmd_logs_resp);

            if (resp->result!=0) {
                ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, resp->result_info);
                return E_DEFAULT;
            }

            if (req->sub_op=="result") {
                auto n = resp->peer_node_logs.size();

                if (n==0) {

                    ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, "training result is empty");
                    return E_DEFAULT;
                }

                if (n!=1) {

                    ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, "training result is invalid");
                    return E_DEFAULT;
                }

                auto hash = resp->get_training_result_hash_from_log(resp->peer_node_logs[0].log_content);

                if (hash.empty()) {

                    ERROR_REPLY(HTTP_OK, RPC_RESPONSE_ERROR, "not find training result hash");
                    return E_DEFAULT;
                }
                data.AddMember("training_result_file", STRING_REF(hash), allocator);

                SUCC_REPLY(data);
                return E_SUCCESS;
            } else {
                rapidjson::Value peer_node_logs(rapidjson::kArrayType);
                for (auto s = resp->peer_node_logs.begin(); s!=resp->peer_node_logs.end(); s++) {
                    rapidjson::Value log(rapidjson::kObjectType);
                    log.AddMember("peer_node_id", STRING_REF(s->peer_node_id), allocator);
                    log.AddMember("log_content", STRING_REF(s->log_content), allocator);

                    peer_node_logs.PushBack(log, allocator);
                }
                data.AddMember("peer_node_logs", peer_node_logs, allocator);
                SUCC_REPLY(data);
                return E_SUCCESS;
            }

        }

        std::shared_ptr<message> rest_log(HTTP_REQUEST_PTR httpReq, const std::string& path)
        {
            std::vector<std::string> path_list;
            rest_util::split_path(path, path_list);

            if (path_list.size()!=2) {
                ERROR_REPLY(HTTP_BADREQUEST,
                            RPC_INVALID_PARAMS,
                            "Invalid path_list. GET /api/v1/tasks/{task_id}/trace?flag=tail&line_num=100");
                return nullptr;
            }

            const std::string& task_id = path_list[0];

            std::map<std::string, std::string> query_table;
            rest_util::split_path_into_kvs(path, query_table);

            std::string head_or_tail = query_table["flag"];
            std::string number_of_lines = query_table["line_num"];

            if (head_or_tail.empty()) {
                head_or_tail = "tail";
                number_of_lines = "100";
            } else if (head_or_tail=="head" || head_or_tail=="tail") {
                if (number_of_lines.empty()) {
                    number_of_lines = "100";
                }
            } else {
                ERROR_REPLY(HTTP_BADREQUEST,
                            RPC_INVALID_PARAMS,
                            "Invalid path_list.GET /api/v1/tasks/{task_id}/trace?flag=tail&line_num=100");
                return nullptr;
            }

            std::shared_ptr<cmd_logs_req> req = std::make_shared<cmd_logs_req>();
            req->task_id = task_id;

            uint64_t lines = (uint64_t) std::stoull(number_of_lines);
            if (lines > MAX_NUMBER_OF_LINES) {
                lines = MAX_NUMBER_OF_LINES - 1;
            }
            req->number_of_lines = (uint16_t) lines;


            //tail or head
            if (head_or_tail=="tail") {
                req->head_or_tail = GET_LOG_TAIL;
            } else if (head_or_tail=="head") {
                req->head_or_tail = GET_LOG_HEAD;
            }

            RETURN_REQ_MSG(cmd_logs_req);

        }

        std::shared_ptr<message> rest_api_version(HTTP_REQUEST_PTR httpReq, const std::string& path)
        {
            rapidjson::Document document;
            rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

            rapidjson::Value data(rapidjson::kObjectType);

            data.AddMember("version", STRING_REF(REST_API_VERSION), allocator);

            SUCC_REPLY(data);
            return nullptr;
        }

    }
}




