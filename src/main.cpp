#include <functional>
#include <chrono>
#include "log/log.h"
#include <csignal>
#include "server/server.h"

high_resolution_clock::time_point server_start_time;
std::unique_ptr<server> g_server(new server());

void signal_usr1_handler(int)
{
    g_server->exit();
    close(STDIN_FILENO);
}

void register_signal_function(int signal, void(*handler)(int))
{
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(signal, &sa, nullptr);
}

void init_signal()
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    //signal(SIGHUP, SIG_IGN);  // dbc in daemon process alwasy ignore SIGHUP; and dbc client in normal process should terminate with SIGHUP.
    //signal(SIGTTIN, SIG_IGN);
    //signal(SIGTTOU, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);  // ignore job control signal, e.g. Ctrl-z

    register_signal_function(SIGUSR1, signal_usr1_handler);
}

void init_locale() {
    try
    {
        std::locale("");
    }
    catch (const std::runtime_error&) {
        setenv("LC_ALL", "C", 1);
    }

    std::locale loc = fs::path::imbue(std::locale::classic());
    fs::path::imbue(loc);
}

int main(int argc, char* argv[])
{
    init_signal();
    init_locale();

    server_start_time = high_resolution_clock::now();

    int32_t ret = log::init();
    if (ret != E_SUCCESS)
    {
        return 0;
    }

    srand((int)time(0));

    int result = g_server->init(argc, argv);
    if (E_SUCCESS != result)
    {
        LOG_ERROR << "server init exited, error code: " << result;
        g_server->exit();
        return 0;
    }

    g_server->idle();
    LOG_INFO << "dbc start to exit...";
    g_server->exit();

    LOG_DEBUG << "------dbc shut down------";
    return 0;
}
