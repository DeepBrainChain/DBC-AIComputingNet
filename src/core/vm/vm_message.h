/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        vm_message.h
* description    vm message definition
* date                  2021.04.27
* author            barry
**********************************************************************************/

#pragma once

#include "common.h"
#include "container_message.h"

namespace matrix
{
    namespace core
    {

        class vm_port
        {
        public:

            std::string scheme;

            std::string port;

            std::string host_ip;

            std::string host_port;

        };

        class vm_ulimits
        {
        public:
            vm_ulimits(std::string name, int32_t soft, int32_t hard):m_name(name), m_soft(soft), m_hard(hard)
            {
            }
            std::string m_name;
            int32_t m_soft;
            int32_t m_hard;
        };

        class vm_ports
        {
        public:

            std::map<std::string, vm_port> ports;

        };

        class vm_mount
        {
        public:

            std::string target;

            std::string source;

            //std::string destination;
            std::string type;

            //std::string driver;

            //std::string mode;

            //bool rw;

            //std::string propagation;

            bool read_only;

            std::string consistency;   //default, consistent, cached, or delegated

        };

        class vm_device
        {
        public:

            std::string path_on_host;

            std::string path_in_vm;

            std::string cgroup_permissions;

        };

        class vm_host_config
        {
        public:

            //GPU needed
            std::list<std::string> binds;

            //GPU needed
            std::list<vm_device> devices;

            //GPU needed
            std::string volume_driver;

            //GPU needed
            std::list<vm_mount> mounts;

            std::string vm_id_file;  //not in create vm config

            std::list<lxc_conf> lxc_confs;  //not in create vm config

            std::list<std::string> links;

            vm_ports port_bindings;

            bool privileged;

            bool publish_all_ports;

            std::string dns;

            std::string dns_search;

            std::list<std::string> volumes_from;

            int64_t  nano_cpus;

            int64_t memory_swap;

            std::string autodbcimage_version;

            int64_t memory;

            int64_t disk_quota;

            std::string storage;

            int32_t cpu_shares;

            int32_t cpu_period;

            int32_t cpu_quota;

            int64_t share_memory;
            
            std::list<vm_ulimits> ulimits;

            //nvidia docker 2.0
            std::string runtime;
        };

        class update_vm_config : public json_io_buf
        {
        public:

            int64_t memory;

            int64_t memory_swap;

            int32_t cpu_shares;
            int32_t cpu_period;
            int32_t cpu_quota;
            int32_t gpus;
            std::string storage;
            int32_t sleep_time;
            //GPU needed
            std::list<std::string> env;

            int64_t disk_quota;

            std::string update_to_string();

        };

        class vm_config : public json_io_buf
        {
        public:

            vm_config();

            std::string host_name;

            std::list<std::string> port_specs;

            std::string user;

            bool tty;

            bool stdin_open;

            bool stdin_once;

            int64_t memory_limit;

            //int64_t memory_swap;

            int32_t cpu_shares;

            bool attach_stdin;

            bool attach_stdout;

            bool attach_stderr;

            //GPU needed
            std::list<std::string> env;

            std::list<std::string> cmd;

            std::list<std::string> dns;

            //GPU needed
            std::string image;

            bound_host_volumes volumes;

            //std::list<std::string> volumes_from;

            std::list<std::string> entry_point;

            bool network_disabled;

            //bool privileged;

            std::string working_dir;

            std::string domain_name;

            std::map<std::string, std::string> exposed_ports;

            std::list<int32_t> on_build;

            vm_host_config host_config;

            std::string to_string();

        };

        class vm_create_resp : public json_io_buf
        {
        public:

            std::string vm_id;

            std::list<std::string> warnings;

            void from_string(const std::string & buf);

        };

         class vm_state
        {
        public:

            vm_state() : running(false), pid(0), exit_code(999), ghost(false) {}

            bool running;

            int pid;

            int exit_code;

            std::string started_at;

            bool ghost;

            std::string finished_at;
        };

        class vm_network_settings
        {
        public:

            std::string ip_address;

            int ip_prefix_len;

            std::string gateway;

            std::string bridge;

            std::map<std::string, std::map<std::string, std::string>> port_mapping;

            vm_ports ports;

        };

        class vm_inspect_response
        {
        public:

            std::string id;

            std::string created;

            std::string path;

            std::list<std::string> args;

            vm_config config;

            vm_state state;

            std::string image_id;

            vm_network_settings network_settings;

            std::string sys_init_path;

            std::string resolv_conf_path;

            std::map<std::string, std::string> volumes;

            std::map<std::string, std::string> volumes_rw;

            std::string host_name_path;

            std::string hosts_path;

            std::string name;

            std::string driver;

            vm_host_config host_config;

            std::string exec_driver;

            std::string mount_label;

            void from_string(const std::string & buf);

        };

        class vm_logs_req
        {
        public:

            std::string vm_id;

            int8_t head_or_tail;

            int16_t number_of_lines;

            bool timestamps;
        };

        class vm_logs_resp
        {
        public:

            std::string log_content;
        };

        class vm_info
        {
        public:
            std::string id = "";
            std::string root_dir = "";
            std::map<std::string, std::string> runtimes;

            void from_string(const std::string & buf);
        };

    }

}