#include "env_manager.h"
#include <event2/thread.h>
#include <event2/event.h>
#include <event2/http.h>
#include <cstdlib>

ERR_CODE env_manager::init() {
    init_locale();

    init_endian_type();

    init_core_path();

    init_libevent_config();

    return E_SUCCESS;
}

void env_manager::init_endian_type() {
    uint32_t a = 0x12345678;
    uint8_t *p = (uint8_t *) (&a);
    m_endian_type = (*p == 0x78) ? little_endian : big_endian;
}

void env_manager::init_core_path() {
    m_home_path = util::get_exe_dir();

    //conf file full path
    m_conf_path = m_home_path;
    m_conf_path /= boost::filesystem::path(CONF_DIR_NAME);
    m_conf_path /= boost::filesystem::path(CONF_FILE_NAME);

    //dat file full path
    m_dat_path = m_home_path;
    m_dat_path /= boost::filesystem::path(DAT_DIR_NAME);

    //db file full path
    m_db_path = m_home_path;
    m_db_path /= boost::filesystem::path(DB_DIR_NAME);

    //addr file full path
    m_peer_path = m_home_path;
    m_peer_path /= boost::filesystem::path(CONF_DIR_NAME);
    m_peer_path /= boost::filesystem::path(PEER_FILE_NAME);

    //tool full path
    m_tool_path = m_home_path;
    m_tool_path /= boost::filesystem::path(TOOL_DIR_NAME);
}

void env_manager::init_locale() {
    try {
        std::locale("");
    }
    catch (const std::runtime_error &) {
        setenv("LC_ALL", "C", 1);
    }

    std::locale loc = boost::filesystem::path::imbue(std::locale::classic());
    boost::filesystem::path::imbue(loc);
}

void env_manager::init_libevent_config() {
    event_enable_debug_logging(EVENT_DBG_NONE);
    evthread_use_pthreads();
}
