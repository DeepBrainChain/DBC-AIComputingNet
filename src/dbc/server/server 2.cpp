#include "server.h"
#include "server_initiator.h"

std::unique_ptr<server> g_server(new server());

void dummy_idle_task()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(DEFAULT_SLEEP_MILLI_SECONDS));
    //LOG_DEBUG << "main thread do idle cycle task";
}

server::server()
    : m_init_result(E_SUCCESS)
    , m_exited(false)
    , m_module_manager(new module_manager())
    , m_idle_task(dummy_idle_task)
{
}

int32_t server::init(int argc, char* argv[])
{
    m_initiator = std::shared_ptr<server_initiator>(m_init_factory.create_initiator());
    return (m_init_result = m_initiator->init(argc, argv));
}

int32_t server::idle()
{
    while (!m_exited)
    {
        do_cycle_task();
    }

    return E_SUCCESS;
}

void server::do_cycle_task()
{
    if (m_idle_task)
    {
        m_idle_task();
    }
}

int32_t server::exit()
{
    m_module_manager->stop();
    m_module_manager->exit();

    return E_SUCCESS;                           //release resources with smart pointer
}
