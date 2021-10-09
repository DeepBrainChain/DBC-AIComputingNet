#ifndef DBC_SERVER_H
#define DBC_SERVER_H

#include "util/utils.h"
#include "config/conf_manager.h"
#include "service_module/topic_manager.h"
#include "network/connection_manager.h"
#include "server_info.h"
#include "util/utils/crypto_service.h"
#include "timer/timer_matrix_manager.h"
#include <boost/program_options.hpp>

class server {
public:
    server() = default;

    virtual ~server() = default;

    int32_t init(int argc, char *argv[]);

    void idle();

    void exit();

    server_info &get_server_info() { return m_server_info; }

    void add_service_list(const std::string& s) {
        m_server_info.add_service_list(s);
    }

protected:
    virtual int32_t parse_command_line(int argc, const char* const argv[], boost::program_options::variables_map &vm);

    virtual int32_t on_cmd_init();

    virtual int32_t on_daemon();

private:
    bool m_stop = false;
    server_info m_server_info;

    bool m_daemon = false;
    crypto_service m_crypto;
    std::shared_ptr<timer_matrix_manager> m_timer_matrix_manager = nullptr;
};

#endif
