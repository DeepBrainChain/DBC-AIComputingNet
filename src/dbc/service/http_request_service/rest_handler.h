/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : rest_handler.h
* description       : rest callback function
* date              : 2018.12.1
* author            : tower && taz
**********************************************************************************/

#ifndef REST_HANDLER_H
#define REST_HANDLER_H

#include "http_server.h"
#include "api_call.h"
#include "message.h"

using namespace matrix::core;

namespace dbc {
    static const std::string REST_API_VERSION = "v1.1";
    static const std::string REST_API_URI = "/api/v1";

    std::shared_ptr<message> rest_task(const HTTP_REQUEST_PTR &httpReq, const std::string &path);

    // create
    std::shared_ptr<message> rest_create_task(const HTTP_REQUEST_PTR &httpReq, const std::string &path);

    int32_t on_cmd_create_task_rsp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg);

    // start <task_id>
    std::shared_ptr<message> rest_start_task(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_cmd_start_task_rsp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg);

    // restart <task_id>
    std::shared_ptr<message> rest_restart_task(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_cmd_restart_task_rsp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg);

    // stop <task_id>
    std::shared_ptr<message> rest_stop_task(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_cmd_stop_task_rsp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg);

    // clean
    std::shared_ptr<message> rest_clean_task(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_cmd_clean_task_rsp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg);

    // result / trace
    std::shared_ptr<message> rest_task_result(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    std::shared_ptr<message> rest_task_trace(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_cmd_task_logs_rsp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg);

    // list
    std::shared_ptr<message> rest_list_task(const HTTP_REQUEST_PTR &httpReq, const std::string &path);

    std::shared_ptr<message> rest_show_task_info(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_cmd_list_task_rsp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg);

    // peers
    std::shared_ptr<message> rest_peers(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_cmd_get_peer_nodes_rsp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg);

    // /stat/
    std::shared_ptr<message> rest_stat(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    // /mining_nodes/
    std::shared_ptr<message> rest_mining_nodes(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_list_node_rsp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg);

    // /config/
    std::shared_ptr<message> rest_config(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    // /
    std::shared_ptr<message> rest_api_version(const HTTP_REQUEST_PTR& httpReq, const std::string &path);
}

#endif
