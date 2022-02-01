#include <functional>
#include <chrono>
#include "log/log.h"
#include <csignal>
#include "server/server.h"

server g_server;

void signal_usr1_handler(int)
{
    g_server.exit();
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
    register_signal_function(SIGUSR1, signal_usr1_handler);
}

void init_locale() {
    try {
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

    srand((int)time(0));

    int result = g_server.init(argc, argv);
    if (E_SUCCESS != result) {
        std::cout << "server init failed: " << result;
        g_server.exit();
        return 0;
    }

    g_server.idle();
    g_server.exit();

    return 0;
}
