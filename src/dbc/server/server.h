#ifndef DBC_SERVER_H
#define DBC_SERVER_H

#include "util/utils.h"
#include "config/conf_manager.h"
#include "service_module/topic_manager.h"
#include "network/connection_manager.h"
#include "timer/timer_matrix_manager.h"

class Server {
public:
    Server() = default;

    virtual ~Server() = default;

    ERRCODE Init(int argc, char *argv[]);

    void Idle();

    void Exit();

public:
	static DBC_NODE_TYPE NodeType;

    static std::string NodeName;

protected:
    ERRCODE InitCrypto(bpo::variables_map &options);

    ERRCODE ExitCrypto();

    ERRCODE ParseCommandLine(int argc, char* argv[], bpo::variables_map &options);

    void Daemon();

private:
    std::atomic<bool> m_running {true};
    std::shared_ptr<timer_matrix_manager> m_timer_matrix_manager = nullptr;
};

#endif
