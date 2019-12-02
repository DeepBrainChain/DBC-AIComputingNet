/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        container_message.h
* description    container message definition
* date                  2018.04.07
* author            Bruce Feng
**********************************************************************************/

#pragma once

#include "common.h"


#define STRING_REF(VAR)                        rapidjson::StringRef(VAR.c_str(), VAR.length())
#define STRING_DUP(VAR)                        rapidjson::Value().SetString(VAR.c_str(),VAR.length(),allocator)

#define RUNTIME_NVIDIA                                  "nvidia"
#define RUNTIME_DEFAULT                                 "runc"

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

            std::list<std::string> binds;

            std::list<std::string> dests;

            std::list<std::string> modes;

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

        class container_ulimits
        {
        public:
            container_ulimits(std::string name, int32_t soft, int32_t hard):m_name(name), m_soft(soft), m_hard(hard)
            {
            }
            std::string m_name;
            int32_t m_soft;
            int32_t m_hard;
        };

        class container_ports
        {
        public:

            std::map<std::string, container_port> ports;

        };

        class container_mount
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

        class container_device
        {
        public:

            std::string path_on_host;

            std::string path_in_container;

            std::string cgroup_permissions;

        };

        class container_host_config
        {
        public:

            //GPU needed
            std::list<std::string> binds;

            //GPU needed
            std::list<container_device> devices;

            //GPU needed
            std::string volume_driver;

            //GPU needed
            std::list<container_mount> mounts;

            std::string container_id_file;  //not in create container config

            std::list<lxc_conf> lxc_confs;  //not in create container config

            std::list<std::string> links;

            container_ports port_bindings;

            bool privileged;

            bool publish_all_ports;

            std::string dns;

            std::string dns_search;

            std::list<std::string> volumes_from;

            int64_t  nano_cpus;

            int64_t memory_swap;
            
            int64_t memory;

            std::string storage;

            int32_t cpu_shares;

            int64_t share_memory;
            
            std::list<container_ulimits> ulimits;

            //nvidia docker 2.0
            std::string runtime;
        };

        class update_container_config : public json_io_buf
        {
        public:



            int64_t memory;

            int64_t memory_swap;

            int32_t cpu_shares;

            int32_t gpus;

            //GPU needed
         //   std::list<std::string> env;

            int64_t disk_quota;

            std::string update_to_string();

        };

        class container_config : public json_io_buf
        {
        public:

            container_config();

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

            container_host_config host_config;

            std::string to_string();

        };

        class container_create_resp : public json_io_buf
        {
        public:

            std::string container_id;

            std::list<std::string> warnings;

            void from_string(const std::string & buf);

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

        class container_logs_req
        {
        public:

            std::string container_id;

            int8_t head_or_tail;

            int16_t number_of_lines;

            bool timestamps;
        };

        class container_logs_resp
        {
        public:

            std::string log_content;
        };

        class nvidia_config_resp
        {
        public:

            //--volume-driver=nvidia-docker --volume=nvidia_driver_384.111:/usr/local/nvidia:ro --device=/dev/nvidiactl --device=/dev/nvidia-uvm --device=/dev/nvidia-uvm-tools --device=/dev/nvidia0
            std::string content;
        };

        class docker_info
        {
        public:
            std::string id = "";
            std::string root_dir = "";
            std::map<std::string, std::string> runtimes;

            void from_string(const std::string & buf);
        };

    }

}