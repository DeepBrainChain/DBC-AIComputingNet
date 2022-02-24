#ifndef DBC_NODE_MONITOR_SERVICE_H
#define DBC_NODE_MONITOR_SERVICE_H

#include "util/utils.h"
#include <leveldb/db.h>
#include "service_module/service_module.h"
#include "service/task/TaskMonitorInfo.h"

#define MONITOR_DATA_SENDER_TASK_TIMER                                   "monitor_data_sender_task"

#define DBC_ZABBIX_SERVER_IP      "116.169.53.132"
#define DBC_ZABBIX_SERVER_PORT    "10051"

struct monitor_server {
    std::string ip;
    std::string port;
};

class node_monitor_service : public service_module, public Singleton<node_monitor_service> {
public:
    node_monitor_service() = default;

    ~node_monitor_service() override;

    int32_t init(bpo::variables_map &options);

protected:
    void init_timer() override;

    void init_invoker() override;

    void init_subscription() override;

    void on_monitor_data_sender_task_timer(const std::shared_ptr<core_timer>& timer);

    void update_monitor_data();

    void send_monitor_data();

private:
    uint32_t m_monitor_data_sender_task_timer_id = INVALID_TIMER_ID;

    std::vector<monitor_server> m_monitor_servers;

    std::map<std::string, dbcMonitor::domMonitorData> m_monitor_datas;

};

#endif
