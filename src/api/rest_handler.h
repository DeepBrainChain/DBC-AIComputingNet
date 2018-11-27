/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : rest_handler.h
* description       : rest callback function
* date              : 2018.11.9
* author            : tower && taz
**********************************************************************************/

#ifndef REST_HANDLER_H
#define REST_HANDLER_H

#include "http_server.h"


using namespace matrix::core;

namespace ai
{
    namespace dbc
    {
        
        bool rest_peers(http_request *httpReq, const std::string &path);
        
        bool rest_mining_nodes(http_request *httpReq, const std::string &path);
        
        bool rest_task(http_request *httpReq, const std::string &path);
        
        bool rest_log(http_request *httpReq, const std::string &path);
        
        bool rest_task_start(http_request *httpReq, const std::string &path);
        
        bool rest_task_stop(http_request *httpReq, const std::string &path);
        
        bool rest_task_list(http_request *httpReq, const std::string &path);
        
        bool rest_task_info(http_request *httpReq, const std::string &path);
        
        bool rest_task_result(http_request *httpReq, const std::string &path);
        
        bool rest_api_version(http_request *httpReq, const std::string &path);
        
        
        
        
        
        
        //
        //This module outputs the following constants
        //
        
        static const std::string REST_API_VERSION = "v0.3.5.2";
        static const std::string REST_API_URI = "/api/v1";
        
        static const http_path_handler uri_prefixes[] = {
                {"/peers",        false, rest_peers},
                {"/mining_nodes", false, rest_mining_nodes},
                {"/tasks",        false, rest_task},
                {"",              true,  rest_api_version},
            
        };
        
    }  // namespace dbc
}  // namespace ai


#endif
