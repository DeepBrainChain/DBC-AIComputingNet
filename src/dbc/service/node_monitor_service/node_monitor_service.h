#ifndef DBC_NODE_MONITOR_SERVICE_H
#define DBC_NODE_MONITOR_SERVICE_H

#include "util/utils.h"
#include <leveldb/db.h>
#include "service_module/service_module.h"
#include "task/detail/info/TaskInfoManager.h"
#include "task/detail/wallet_rent_task/WalletRentTaskManager.h"
#include "task/detail/TaskMonitorInfo.h"
#include "task/HttpDBCChainClient.h"
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

    ERRCODE init() override;

	void exit() override;

    void listMonitorServer(const std::string& wallet, std::vector<monitor_server>& servers) const;

    FResult setMonitorServer(const std::string& wallet, const std::string& additional);

    // update every 3 min
    std::string curRenterWallet() const { return m_cur_renter_wallet; }

protected:
    void init_timer() override;

    void init_invoker() override;

    int32_t init_db();

    int32_t load_wallet_monitor_from_db();

    void on_monitor_data_sender_task_timer(const std::shared_ptr<core_timer>& timer);

    void on_update_cur_renter_wallet_timer(const std::shared_ptr<core_timer>& timer);

    void send_monitor_data(const dbcMonitor::domMonitorData& dmData, const monitor_server& server) const;

    void send_monitor_data(const dbcMonitor::hostMonitorData& hmData, const monitor_server& server) const;

private:
    uint32_t m_monitor_data_sender_task_timer_id = INVALID_TIMER_ID;
    uint32_t m_update_cur_renter_wallet_timer_id = INVALID_TIMER_ID;

    // deepbranchain的监控
    monitor_server m_dbc_monitor_server;

    // 矿工的监控
    monitor_server m_miner_monitor_server;

    // 各个虚拟机的监控数据
    std::map<std::string, dbcMonitor::domMonitorData> m_monitor_datas;

    std::shared_ptr<leveldb::DB> m_wallet_monitors_db;

    // 租用人的监控
    std::map<std::string, std::vector<monitor_server>> m_wallet_monitors;

    // 当前租用人的钱包地址
    std::string m_cur_renter_wallet;

};

#endif
