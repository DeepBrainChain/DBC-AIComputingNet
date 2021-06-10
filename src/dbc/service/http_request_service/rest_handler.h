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
#include "api_call_handler.h"
#include "api_call.h"

using namespace matrix::core;

namespace dbc {
    static const std::string REST_API_VERSION = "v1.1";
    static const std::string REST_API_URI = "/api/v1";

    std::shared_ptr<message> rest_task(const HTTP_REQUEST_PTR &httpReq, const std::string &path);

    std::shared_ptr<message> rest_task_list(const HTTP_REQUEST_PTR &httpReq, const std::string &path);

    std::shared_ptr<message> rest_task_info(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_list_training_resp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg);

    std::shared_ptr<message> rest_task_start(const HTTP_REQUEST_PTR &httpReq, const std::string &path);

    int32_t on_start_training_resp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg);

    std::shared_ptr<message> rest_task_stop(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    std::shared_ptr<message> rest_task_restart(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_stop_training_resp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg);

    std::shared_ptr<message> rest_task_result(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    std::shared_ptr<message> rest_log(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_logs_resp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg);

    std::shared_ptr<message> rest_api_version(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    std::shared_ptr<message> rest_stat(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    std::shared_ptr<message> rest_mining_nodes(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_show_resp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg);

    std::shared_ptr<message> rest_peers(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_get_peer_nodes_resp(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg);

    std::shared_ptr<message> rest_task_clean(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    std::shared_ptr<message> rest_config(const HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_task_clean(const HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<message> &resp_msg);
}

#endif
