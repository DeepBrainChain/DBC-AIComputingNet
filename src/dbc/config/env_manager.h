#ifndef DBC_ENV_MANAGER_H
#define DBC_ENV_MANAGER_H

#include <boost/filesystem.hpp>
#include "module/module.h"

extern void signal_usr1_handler(int);

#define CONF_DIR_NAME                           "conf"
#define DAT_DIR_NAME                            "dat"
#define DB_DIR_NAME                             "db"
#define TOOL_DIR_NAME                           "tool"
#define CONF_FILE_NAME                          "core.conf"
#define PEER_FILE_NAME                          "peer.conf"
#define CONTAINER_FILE_NAME                     "container.conf"
#define VM_FILE_NAME                            "vm.conf"
#define DEFAULT_PATH_BUF_LEN                    512

enum endian_type {
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

    static const boost::filesystem::path & get_conf_path() { return m_conf_path; }

    static const boost::filesystem::path & get_dat_path() { return m_dat_path; }

    static const boost::filesystem::path & get_db_path() { return m_db_path; }

    static const boost::filesystem::path & get_peer_path() { return m_peer_path; }

    static const boost::filesystem::path & get_container_path() { return m_container_path; }

    static const boost::filesystem::path & get_vm_path() { return m_vm_path; }

    static const boost::filesystem::path & get_home_path() { return m_home_path;}

    static const boost::filesystem::path & get_tool_path() {return m_tool_path;}

    static endian_type get_endian_type() { return m_endian_type; }

protected:
    void init_core_path();

    void init_signal();

    void init_locale();

    void register_signal_function(int signal, void(*handler)(int));

    void init_libevent_config();

    void update_http_server_logging(bool enable);

    static void init_endian_type();

    static void libevent_log_cb(int severity, const char *msg);

    static boost::filesystem::path m_conf_path;

    static boost::filesystem::path m_dat_path;

    static boost::filesystem::path m_db_path;

    static boost::filesystem::path m_peer_path;

    static boost::filesystem::path m_home_path;
    static boost::filesystem::path m_tool_path;

    static boost::filesystem::path m_container_path;

    static boost::filesystem::path m_vm_path;

    static endian_type m_endian_type;
};

#endif
