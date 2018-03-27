/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºapi_call_handler.h
* description    £ºapi call handler for command line, json rpc
* date                  : 2018.03.25
* author            £º
**********************************************************************************/

#pragma once


#include <boost/program_options.hpp>


using namespace boost::program_options;


namespace ai
{
    namespace dbc
    {

        struct start_training_req
        {

        };

        struct start_training_resp
        {

        };

        struct stop_training_req
        {

        };

        struct stop_training_resp
        {

        };

        struct start_multi_training_req
        {

        };

        struct start_multi_training_resp
        {

        };

        struct list_training_req
        {

        };

        struct list_training_resp
        {

        };

        struct get_peers_req
        {

        };

        struct get_peers_resp
        {

        };

        class api_call_handler
        {
        public:

            api_call_handler() = default;

            ~api_call_handler() = default;

            void start_training(start_training_req &req, start_training_resp &resp, variables_map &call_context);

            void stop_training(stop_training_req &req, stop_training_resp &resp, variables_map &call_context);

            void start_multi_training(start_multi_training_req &req, start_multi_training_resp &resp, variables_map &call_context);

            void list_training(list_training_req &req, list_training_resp &resp, variables_map &call_context);

            void get_peers(get_peers_req &req, get_peers_resp &resp, variables_map &call_context);

        };

    }

}
