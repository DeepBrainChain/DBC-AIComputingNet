#ifndef DBC_DBC_SERVER_INITIATOR_H
#define DBC_DBC_SERVER_INITIATOR_H

#include "util/utils.h"
#include <boost/program_options.hpp>
#include "server_initiator.h"
#include "util/utils/crypto_service.h"

class dbc_server_initiator : public server_initiator
{
public:
    dbc_server_initiator() : m_daemon(false) {}

    virtual ~dbc_server_initiator() = default;

    virtual int32_t init(int argc, char* argv[]);

    virtual int32_t exit();

    virtual int32_t parse_command_line(int argc, const char* const argv[], boost::program_options::variables_map &vm);

    virtual int32_t on_cmd_init();

    virtual int32_t on_daemon();

protected:
    bool m_daemon;
    crypto_service m_crypto;
};

#endif
