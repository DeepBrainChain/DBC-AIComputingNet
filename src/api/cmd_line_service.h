/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         cmd_line_service.h
* description     cmd line api service
* date                   2018.03.25
* author              Bruce Feng
**********************************************************************************/

#pragma once


#include "api_call_handler.h"
#include "module.h"
#include "task_common_def.h"
#include <vector>
#include <string>


using namespace matrix::core;

#define MAX_CMD_LINE_BUF_LEN                            1024
#define MAX_CMD_LINE_ARGS_COUNT                         32
#define CMD_LINE_API_MODULE                             "cmd_line_api_module"


namespace ai
{
    namespace dbc
    {

        class cmd_line_service : public module
        {

            using invoker_type = typename std::function<void( int, char* [] )>;

            typedef std::map<std::string, invoker_type> cmd_invokers;

        public:

            cmd_line_service();

            ~cmd_line_service() = default;

            std::string module_name() const
            {
                return CMD_LINE_API_MODULE;
            }

            int32_t init( bpo::variables_map& options );

            void on_usr_cmd();

        protected:

            void init_cmd_invoker();

            //interact with user command

            void start_training( int argc, char* argv[] );

            void stop_training( int argc, char* argv[] );

            void start_multi_training( int argc, char* argv[] );

            void list_training( int argc, char* argv[] );

            void get_peers( int argc, char* argv[] );

            void logs( int argc, char* argv[] );

            void show( int argc, char* argv[] );

            void clear( int argc, char* argv[] );

            void ps( int argc, char* argv[] );

            void result( int argc, char* argv[] );

            void system_cmd( int argc, char* argv[] );

            void task( int argc, char* argv[] );

            void task_clean( int argc, char* argv[] );

            std::shared_ptr<cmd_start_training_resp> start_training_task_helper(
                    std::shared_ptr<cmd_start_training_req> req );

            void cmd_key(int argc, char* argv[]);


        protected:

            template<typename resp_type> void format_output( std::shared_ptr<resp_type> resp );

        protected:

            //api_call_handler m_handler;

            cmd_invokers m_invokers;

            char m_cmd_line_buf[MAX_CMD_LINE_BUF_LEN];

            std::vector<std::string> m_argvs;

            std::string m_last_task_id;

            std::string m_rl_history_fn;

        };

    }

}
