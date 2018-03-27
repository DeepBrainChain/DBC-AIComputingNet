/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºcmd_line_service.h
* description    £ºcmd line api processor
* date                  : 2018.03.25
* author            £º
**********************************************************************************/

#pragma once


#include "api_call_handler.h"
#include "module.h"


using namespace matrix::core;

#define MAX_CMD_LINE_BUF_LEN                            1024
#define MAX_CMD_LINE_ARGS_COUNT                    32
#define CMD_LINE_API_MODULE                               "cmd_line_api_module"


namespace ai
{
    namespace dbc
    {

        class cmd_line_service : public  module
        {
            
            using invoker_type = typename std::function<void (int, char* [] )>;

            typedef std::map<std::string, invoker_type> processor_invoker;

        public:

            cmd_line_service();

            ~cmd_line_service() = default;

            std::string module_name() const { return CMD_LINE_API_MODULE; }

            int32_t init(bpo::variables_map &options);

            void on_usr_cmd();

        protected:

            //interact with user command

            void start_training(int argc, char* argv[]);

            void stop_training(int argc, char* argv[]);

            void start_multi_training(int argc, char* argv[]);

            void list_training(int argc, char* argv[]);

            void get_peers(int argc, char* argv[]);

        protected:

            api_call_handler m_handler;

            processor_invoker m_invokers;

            char m_cmd_line_buf[MAX_CMD_LINE_BUF_LEN];

            int m_argc;

            char *m_argv[MAX_CMD_LINE_ARGS_COUNT];

        };

    }

}
