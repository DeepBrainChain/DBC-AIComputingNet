#include "start_up.h"
#include <memory>
#include "server.h"
#include "log/log.h"

int main_task(int argc, char* argv[])
{
    int result = g_server->init(argc, argv);
    if (E_SUCCESS != result)
    {
        LOG_ERROR << "server init exited, error code: " << result;
        g_server->exit();

        return result;
    }

    g_server->idle();
    LOG_INFO << "dbc start to exit...";
    g_server->exit();

    LOG_DEBUG << "------dbc shut down------";

    return E_SUCCESS;
}
