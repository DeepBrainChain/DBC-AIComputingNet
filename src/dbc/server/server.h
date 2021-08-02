#ifndef DBC_SERVER_H
#define DBC_SERVER_H

#include "util/utils.h"
#include "config/conf_manager.h"
#include "service_module/topic_manager.h"
#include "network/connection_manager.h"
#include "module/module_manager.h"
#include "server_initiator_factory.h"
#include "server_info.h"

#define DEFAULT_SLEEP_MILLI_SECONDS                 1000

#define TOPIC_MANAGER                                (g_server->get_topic_manager())
#define CONF_MANAGER                                 (g_server->get_conf_manager())
#define CONNECTION_MANAGER                           (g_server->get_connection_manager())
#define P2P_SERVICE                                  (g_server->get_p2p_net_service())

class server;
extern std::unique_ptr<server> g_server;

class server
{
    using init_factory = server_initiator_factory;
    using idle_task_functor_type = std::function<void()>;

public:
    server();

    virtual ~server() = default;

    virtual int32_t init(int argc, char* argv[]);

    virtual int32_t idle();

    virtual int32_t exit();

    virtual bool is_exited() { return m_exited; }

    virtual void set_exited(bool exited = true) { m_exited = exited; }

    virtual bool is_init_ok() { return (m_init_result == E_SUCCESS); }

    void bind_idle_task(idle_task_functor_type functor) { m_idle_task = functor; }

    std::shared_ptr<module_manager> get_module_manager() { return m_module_manager; }

    conf_manager *get_conf_manager() { return (conf_manager *)(m_module_manager->get(conf_manager_name).get()); }

    topic_manager *get_topic_manager() { return (topic_manager *)(m_module_manager->get(topic_manager_name).get()); }

    dbc::network::connection_manager *get_connection_manager() { return (dbc::network::connection_manager *)(m_module_manager->get(connection_manager_name).get()); }

    server_info& get_server_info() { return m_server_info; }
    void add_service_list(std::string s)  { m_server_info.add_service_list(s); }

protected:
    virtual void do_cycle_task();

protected:
    int32_t m_init_result;

    bool m_exited;

    std::shared_ptr<server_initiator> m_initiator;

    std::shared_ptr<module_manager> m_module_manager;

    init_factory m_init_factory;

    idle_task_functor_type m_idle_task;

    server_info m_server_info;  // provide infomation like service list, of the computer node that dbc program controlls.
};

#endif
