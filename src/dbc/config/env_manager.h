#ifndef DBC_ENV_MANAGER_H
#define DBC_ENV_MANAGER_H

#include "util/utils.h"
#include <boost/filesystem.hpp>

#define CONF_DIR_NAME               "conf"
#define DAT_DIR_NAME                "dat"
#define DB_DIR_NAME                 "db"
#define SHELL_DIR_NAME              "shell"
#define LOG_DIR_NAME                "logs"

#define CONF_FILE_NAME              "core.conf"
#define CONTAINER_FILE_NAME         "container.conf"
#define VM_FILE_NAME                "vm.conf"
#define PEER_FILE_NAME              "peer.conf"

#define DAT_FILE_NAME               "node.dat"

enum endian_type {
    unknown_endian = 0,
    big_endian,
    little_endian
};

namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;

class EnvManager : public Singleton<EnvManager> {
public:
    EnvManager() = default;

    virtual ~EnvManager() = default;

    ERRCODE Init();

    const bfs::path &get_exe_path() { return m_exe_path; }

    const bfs::path &get_conf_path() { return m_conf_path; }

    const bfs::path &get_dat_path() { return m_dat_path; }

    const bfs::path &get_db_path() { return m_db_path; }

    const bfs::path &get_shell_path() { return m_shell_path; }

    const bfs::path &get_logs_path() { return m_logs_path; }

    const bfs::path &get_conf_file_path() { return m_conf_file_path; }

    const bfs::path &get_peer_file_path() { return m_peer_file_path; }

    const bfs::path &get_dat_file_path() { return m_dat_file_path; }

    endian_type get_endian_type() { return m_endian_type; }

protected:
    void init_core_path();

    void init_locale();

    void init_libevent_config();

    void init_endian_type();

private:
    bfs::path m_exe_path;

    bfs::path m_conf_path;
    bfs::path m_dat_path;
    bfs::path m_db_path;
    bfs::path m_shell_path;
    bfs::path m_logs_path;

    bfs::path m_conf_file_path;
    bfs::path m_peer_file_path;
    bfs::path m_dat_file_path;

    endian_type m_endian_type = little_endian;
};

#endif
