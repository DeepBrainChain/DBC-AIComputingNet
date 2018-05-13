/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ��container_message.h
* description    ��container message definition
* date                  : 2018.04.07
* author            ��Bruce Feng
**********************************************************************************/

#pragma once

#include "common.h"
#include <stringbuffer.h>


#define STRING_REF(VAR)                        rapidjson::StringRef(VAR.c_str(), VAR.length())


namespace matrix
{
    namespace core
    {

        class json_io_buf
        {
        public:

            virtual ~json_io_buf() = default;

            virtual std::string to_string() { return ""; }

            virtual void from_string(const std::string & buf) { }

        };

        class auth_config
        {
        public:

            std::string user;

            std::string password;

            std::string email;

            std::string server_address;

        };

        class bound_host_volumes
        {
        public:

            std::list<std::string> dests;

            std::list<std::string> binds;

        };

        class container_config : public json_io_buf
        {
        public:

            std::string host_name;

            std::list<std::string> port_specs;

            std::string user;

            bool tty;

            bool stdin_open;

            bool stdin_once;

            int64_t memory_limit;

            int64_t memory_swap;

            int32_t cup_shares;

            bool attach_stdin;

            bool attach_stdout;

            bool attach_stderr;

            std::list<std::string> env;

            std::list<std::string> cmd;

            std::list<std::string> dns;

            std::string image;

            bound_host_volumes volumes;

            std::list<std::string> volumes_from;

            std::list<std::string> entry_point;

            bool network_disabled;

            bool privileged;

            std::string working_dir;

            std::string domain_name;

            std::map<std::string, std::string> exposed_ports;

            std::list<int32_t> on_build;

            std::string to_string();

        };

        class container_create_resp : public json_io_buf
        {
        public:

            std::string container_id;

            std::list<std::string> warnings;

            void from_string(const std::string & buf);

        };

        class lxc_conf
        {
        public:

            std::string key;

            std::string val;

        };

        class container_port
        {
        public:

            std::string scheme;

            std::string port;

            std::string host_ip;

            std::string host_port;

        };

        class container_ports
        {
        public:

            std::map<std::string, container_port> ports;

        };

        class container_host_config
        {
        public:

            std::list<std::string> binds;

            std::string container_id_file;

            std::list<lxc_conf> lxc_confs;

            std::list<std::string> links;

            container_ports port_bindings;

            bool privileged;

            bool publish_all_ports;

            std::string dns;

            std::string dns_search;

            std::string volumes_from;

        };

        class container_state
        {
        public:

            container_state() : running(false), pid(0), exit_code(999), ghost(false) {}

            bool running;

            int pid;

            int exit_code;

            std::string started_at;

            bool ghost;

            std::string finished_at;
        };

        class container_network_settings
        {
        public:

            std::string ip_address;

            int ip_prefix_len;

            std::string gateway;

            std::string bridge;

            std::map<std::string, std::map<std::string, std::string>> port_mapping;

            container_ports ports;

        };

        class container_inspect_response
        {
        public:

            std::string id;

            std::string created;

            std::string path;

            std::list<std::string> args;

            container_config config;

            container_state state;

            std::string image_id;

            container_network_settings network_settings;

            std::string sys_init_path;

            std::string resolv_conf_path;

            std::map<std::string, std::string> volumes;

            std::map<std::string, std::string> volumes_rw;

            std::string host_name_path;

            std::string hosts_path;

            std::string name;

            std::string driver;

            container_host_config host_config;

            std::string exec_driver;

            std::string mount_label;

            void from_string(const std::string & buf);

        };

    }

}