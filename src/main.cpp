#include <functional>
#include <chrono>
#include "log/log.h"
#include <csignal>
#include "server/server.h"

Server g_server;

void signal_handler(int sig) {
	g_server.Exit();
}

void register_signal_function() {
	struct sigaction sa;
	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGUSR1, &sa, nullptr);
	sigaction(SIGUSR2, &sa, nullptr);
	sigaction(SIGINT, &sa, nullptr);
	sigaction(SIGTERM, &sa, nullptr);
}

int main(int argc, char* argv[]) {
	register_signal_function();
	srand((int)time(0));
	int result = g_server.Init(argc, argv);
	if (ERR_SUCCESS != result) {
		std::cout << "server init failed: " << result;
		g_server.Exit();
		return 0;
	}

	g_server.Idle();
	return 0;
}
