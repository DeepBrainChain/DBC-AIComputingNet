/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   dbc_server_initiator.h
* description    :   dbc server initiator for dbc core
* date                  :   2018.01.20
* author            :   Bruce Feng
**********************************************************************************/

#pragma once

#include <boost/program_options.hpp>
#include "server_initiator.h"


namespace ai
{
    namespace dbc
    {
        class dbc_server_initiator : public matrix::core::server_initiator
        {
        public:

            dbc_server_initiator() : m_daemon(false) {}

            virtual ~dbc_server_initiator() = default;

            virtual int32_t init(int argc, char* argv[]);

            virtual int32_t exit();

            //parse command line
            virtual int32_t parse_command_line(int argc, const char* const argv[], boost::program_options::variables_map &vm);

            virtual int32_t on_cmd_init();

            virtual int32_t on_daemon();

        protected:

            bool m_daemon;
        };

    }

}

