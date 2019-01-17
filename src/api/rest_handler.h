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

namespace ai
{
    namespace dbc
    {

        std::shared_ptr<message> rest_task(HTTP_REQUEST_PTR httpReq, const std::string& path);

        std::shared_ptr<message> rest_task_list(HTTP_REQUEST_PTR httpReq, const std::string& path);
        std::shared_ptr<message> rest_task_info(HTTP_REQUEST_PTR httpReq, const std::string& path);

        int32_t on_list_training_resp(HTTP_REQ_CTX_PTR hreq_context, std::shared_ptr<message>& resp_msg);

        std::shared_ptr<message> rest_task_start(HTTP_REQUEST_PTR httpReq, const std::string& path);

        int32_t on_start_training_resp(HTTP_REQ_CTX_PTR hreq_context, std::shared_ptr<message>& resp_msg);

        std::shared_ptr<message> rest_task_stop(HTTP_REQUEST_PTR httpReq, const std::string& path);

        int32_t on_stop_training_resp(HTTP_REQ_CTX_PTR hreq_context, std::shared_ptr<message>& resp_msg);

        std::shared_ptr<message> rest_task_result(HTTP_REQUEST_PTR httpReq, const std::string& path);
        std::shared_ptr<message> rest_log(HTTP_REQUEST_PTR httpReq, const std::string& path);

        int32_t on_logs_resp(HTTP_REQ_CTX_PTR hreq_context, std::shared_ptr<message>& resp_msg);

        std::shared_ptr<message> rest_api_version(HTTP_REQUEST_PTR httpReq, const std::string& path);
        std::shared_ptr<message> rest_stat(HTTP_REQUEST_PTR httpReq, const std::string& path);

        std::shared_ptr<message> rest_mining_nodes(HTTP_REQUEST_PTR httpReq, const std::string& path);

        int32_t on_show_resp(HTTP_REQ_CTX_PTR hreq_context, std::shared_ptr<message>& resp_msg);

        std::shared_ptr<message> rest_peers(HTTP_REQUEST_PTR httpReq, const std::string& path);
        int32_t on_get_peer_nodes_resp(HTTP_REQ_CTX_PTR hreq_context, std::shared_ptr<message>& resp_msg);


        std::shared_ptr<message> rest_task_clean(HTTP_REQUEST_PTR httpReq, const std::string& path);
        std::shared_ptr<message> rest_config(HTTP_REQUEST_PTR httpReq, const std::string& path);
        int32_t on_task_clean(HTTP_REQ_CTX_PTR hreq_context, std::shared_ptr<message>& resp_msg);
        //
        //This module outputs the following constants
        //

        static const std::string REST_API_VERSION = "v1.1";
        static const std::string REST_API_URI = "/api/v1";

        static const http_path_handler uri_prefixes[] = {
            {"/peers", false, rest_peers},
            {"/stat", false, rest_stat},
            {"/mining_nodes", false, rest_mining_nodes},
            {"/tasks", false, rest_task},
            {"/config", false, rest_config},
            { "", true, rest_api_version},
        };

        static const response_msg_handler rsp_handlers[] = {
            {typeid(cmd_get_peer_nodes_resp).name(), on_get_peer_nodes_resp },
            {typeid(cmd_list_training_resp).name(),on_list_training_resp},
            {typeid(cmd_show_resp).name(), on_show_resp},
            {typeid(cmd_start_training_resp).name(), on_start_training_resp},
            {typeid(cmd_stop_training_resp).name(), on_stop_training_resp},
            {typeid(cmd_logs_resp).name(), on_logs_resp},
            {typeid(cmd_task_clean_resp).name(), on_task_clean},
        };

    }  // namespace dbc
}  // namespace ai


#endif
