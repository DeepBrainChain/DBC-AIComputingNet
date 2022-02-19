#include "env_manager.h"
#include <event2/thread.h>
#include <event2/event.h>
#include <event2/http.h>
#include <cstdlib>

ERRCODE EnvManager::Init() {
    init_locale();

    init_endian_type();

    init_core_path();

    init_libevent_config();

    return ERR_SUCCESS;
}

void EnvManager::init_locale() {
    try {
        std::locale("");
    }
    catch (const std::runtime_error &) {
        setenv("LC_ALL", "C", 1);
    }

    std::locale loc = bfs::path::imbue(std::locale::classic());
    bfs::path::imbue(loc);
}

void EnvManager::init_endian_type() {
    uint32_t a = 0x12345678;
    uint8_t *p = (uint8_t *) (&a);
    m_endian_type = (*p == 0x78) ? little_endian : big_endian;
}

void EnvManager::init_core_path() {
    m_exe_path = util::get_exe_dir();

    m_conf_path = m_exe_path;
    m_conf_path /= bfs::path(CONF_DIR_NAME);

    m_dat_path = m_exe_path;
    m_dat_path /= bfs::path(DAT_DIR_NAME);

    m_db_path = m_exe_path;
    m_db_path /= bfs::path(DB_DIR_NAME);

    m_shell_path = m_exe_path;
    m_shell_path /= bfs::path(SHELL_DIR_NAME);

    m_logs_path = m_exe_path;
    m_logs_path /= bfs::path(LOG_DIR_NAME);

    m_conf_file_path = m_exe_path;
    m_conf_file_path /= bfs::path(CONF_DIR_NAME);
    m_conf_file_path /= bfs::path(CONF_FILE_NAME);

    m_peer_file_path = m_exe_path;
    m_peer_file_path /= bfs::path(CONF_DIR_NAME);
    m_peer_file_path /= bfs::path(PEER_FILE_NAME);

    m_dat_file_path = m_exe_path;
    m_dat_file_path /= bfs::path(DAT_DIR_NAME);
    m_dat_file_path /= bfs::path(DAT_FILE_NAME);
}

void EnvManager::init_libevent_config() {
    event_enable_debug_logging(EVENT_DBG_NONE);
    evthread_use_pthreads();
}
