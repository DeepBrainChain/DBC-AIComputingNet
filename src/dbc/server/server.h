#ifndef DBC_SERVER_H
#define DBC_SERVER_H

#include "util/utils.h"
#include "config/conf_manager.h"
#include "network/topic_manager.h"
#include "network/connection_manager.h"
#include "timer/timer_tick_manager.h"

class Server {
public:
    Server() = default;

    virtual ~Server() = default;

    ERRCODE Init(int argc, char *argv[]);

    void Idle();

    void Exit();

public:
	static NODE_TYPE NodeType;

    static std::string NodeName;

protected:
    ERRCODE InitCrypto();

    ERRCODE ExitCrypto();

    ERRCODE ParseCommandLine(int argc, char* argv[]);

    ERRCODE Daemon();

private:
    std::atomic<bool> m_running {true};
    std::shared_ptr<timer_tick_manager> m_timer_matrix_manager = nullptr;
};

#endif
