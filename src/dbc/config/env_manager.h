#ifndef DBC_ENV_MANAGER_H
#define DBC_ENV_MANAGER_H

#include "util/utils.h"
#include <boost/filesystem.hpp>

#define CONF_DIR_NAME                           "conf"
#define DAT_DIR_NAME                            "dat"
#define DB_DIR_NAME                             "db"
#define TOOL_DIR_NAME                           "tool"
#define CONF_FILE_NAME                          "core.conf"
#define PEER_FILE_NAME                          "peer.conf"
#define CONTAINER_FILE_NAME                     "container.conf"
#define VM_FILE_NAME                            "vm.conf"

enum endian_type {
    unknown_endian = 0,
    big_endian,
    little_endian
};

namespace bpo = boost::program_options;

class env_manager : public Singleton<env_manager> {
public:
    env_manager() = default;

    virtual ~env_manager() = default;

    ERR_CODE init();

    const boost::filesystem::path &get_conf_path() { return m_conf_path; }

    const boost::filesystem::path &get_dat_path() { return m_dat_path; }

    const boost::filesystem::path &get_db_path() { return m_db_path; }

    const boost::filesystem::path &get_peer_path() { return m_peer_path; }

    const boost::filesystem::path &get_home_path() { return m_home_path; }

    const boost::filesystem::path &get_tool_path() { return m_tool_path; }

    endian_type get_endian_type() { return m_endian_type; }

protected:
    void init_core_path();

    void init_locale();

    void init_libevent_config();

    void init_endian_type();

private:
    boost::filesystem::path m_conf_path;

    boost::filesystem::path m_dat_path;

    boost::filesystem::path m_db_path;

    boost::filesystem::path m_peer_path;

    boost::filesystem::path m_home_path;

    boost::filesystem::path m_tool_path;

    endian_type m_endian_type = little_endian;
};

#endif
