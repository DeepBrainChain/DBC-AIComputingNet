/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        env_manager.hpp
* description    env manager for dbc core
* date                  :   2018.01.20
* author            Bruce Feng
**********************************************************************************/
#pragma once

#include <boost/filesystem.hpp>
#include "module.h"


namespace fs = boost::filesystem;

extern void signal_usr1_handler(int);


#define CONF_DIR_NAME                           "conf"
#define DAT_DIR_NAME                            "dat"
#define DB_DIR_NAME                             "db"
#define CONF_FILE_NAME                          "core.conf"
#define PEER_FILE_NAME                          "peer.conf"
#define CONTAINER_FILE_NAME                         "container.conf"

#define DEFAULT_PATH_BUF_LEN                    512


namespace matrix
{
    namespace core
    {
        enum endian_type
        {
            unknown_endian =0,
            big_endian,
            little_endian
        };

        class env_manager : public module
        {
        public:

            env_manager() = default;

            virtual ~env_manager() = default;

            virtual int32_t init(bpo::variables_map &options);

            virtual std::string module_name() const { return env_manager_name; };

        public:

            static const fs::path & get_conf_path() { return m_conf_path; }

            static const fs::path & get_dat_path() { return m_dat_path; }

            static const fs::path & get_db_path() { return m_db_path; }

            static const fs::path & get_peer_path() { return m_peer_path; }

            static const fs::path & get_container_path() { return m_container_path; }

            static const fs::path & get_home_path() { return m_home_path;}

            static endian_type get_endian_type() { return m_endian_type; }

        protected:

            void init_core_path();
#if 0
            void init_core_path_with_os_func();
#endif

            void init_signal();

            void init_locale();

            void register_signal_function(int signal, void(*handler)(int));

			void init_libevent_config();

			void update_http_server_logging(bool enable);

			static void init_endian_type();

			static void libevent_log_cb(int severity, const char *msg);

        protected:

            static fs::path m_conf_path;

            static fs::path m_dat_path;

            static fs::path m_db_path;

            static fs::path m_peer_path;

            static fs::path m_home_path;

            static fs::path m_container_path;

            static endian_type m_endian_type;

        };

    }

}
