#ifndef DBC_NODE_MONITOR_SERVICE_H
#define DBC_NODE_MONITOR_SERVICE_H

#include "util/utils.h"
#include <leveldb/db.h>
#include "service_module/service_module.h"
#include "service/task/TaskMonitorInfo.h"
#include "service/task/HttpChainClient.h"
#include "hostMonitorInfo.h"

#define MONITOR_DATA_SENDER_TASK_TIMER                                   "monitor_data_sender_task"
#define UPDATE_CUR_RENTER_WALLET_TIMER                                   "update_cur_renter_wallet"

struct monitor_server {
    std::string host;
    std::string port;
};

class node_monitor_service : public service_module, public Singleton<node_monitor_service> {
public:
    node_monitor_service() = default;

    ~node_monitor_service() override;

    int32_t init(bpo::variables_map &options);

    void listMonitorServer(const std::string& wallet, std::vector<monitor_server>& servers) const;

    FResult setMonitorServer(const std::string& wallet, const std::string& additional);

    // update every 3 min
    std::string curRenterWallet() const { return m_cur_renter_wallet; }

protected:
    void init_timer() override;

    void init_invoker() override;

    void init_subscription() override;

    int32_t init_db();

    int32_t load_wallet_monitor_from_db();

    void on_monitor_data_sender_task_timer(const std::shared_ptr<core_timer>& timer);

    void on_update_cur_renter_wallet_timer(const std::shared_ptr<core_timer>& timer);

    void send_monitor_data(const dbcMonitor::domMonitorData& dmData, const monitor_server& server) const;

    void send_monitor_data(const dbcMonitor::hostMonitorData& hmData, const monitor_server& server) const;

private:
    uint32_t m_monitor_data_sender_task_timer_id = INVALID_TIMER_ID;
    uint32_t m_update_cur_renter_wallet_timer_id = INVALID_TIMER_ID;

    monitor_server m_dbc_monitor_server;

    std::map<std::string, dbcMonitor::domMonitorData> m_monitor_datas;

    std::shared_ptr<leveldb::DB> m_wallet_monitors_db;

    std::map<std::string, std::vector<monitor_server>> m_wallet_monitors;

    std::string m_cur_renter_wallet;

    HttpChainClient m_httpclient;

};

#endif
