#include "node_monitor_service.h"
#include "zabbixSender.h"
#include "service/task/TaskInfoManager.h"
#include "service/task/vm/vm_client.h"
#include "common/version.h"
#include "log/log.h"
#include "server/server.h"

node_monitor_service::~node_monitor_service() {
    if (Server::NodeType == DBC_NODE_TYPE::DBC_COMPUTE_NODE) {
        remove_timer(m_monitor_data_sender_task_timer_id);
    }
}

int32_t node_monitor_service::init(bpo::variables_map &options) {
    service_module::init();

    m_monitor_servers.push_back({DBC_ZABBIX_SERVER_IP, DBC_ZABBIX_SERVER_PORT});
    return ERR_SUCCESS;
}

void node_monitor_service::init_timer() {
    if (Server::NodeType == DBC_NODE_TYPE::DBC_COMPUTE_NODE) {
        // 1min
        // 10s
        m_timer_invokers[MONITOR_DATA_SENDER_TASK_TIMER] = std::bind(&node_monitor_service::on_monitor_data_sender_task_timer, this, std::placeholders::_1);
        m_monitor_data_sender_task_timer_id = this->add_timer(MONITOR_DATA_SENDER_TASK_TIMER, 10 * 1000, ULLONG_MAX, "");
    }
}

void node_monitor_service::init_invoker() {

}

void node_monitor_service::init_subscription() {

}

void node_monitor_service::on_monitor_data_sender_task_timer(const std::shared_ptr<core_timer>& timer) {
    update_monitor_data();
    send_monitor_data();
}

void node_monitor_service::update_monitor_data() {
    const std::map<std::string, std::shared_ptr<dbc::TaskInfo>> task_list = TaskInfoMgr::instance().getTasks();
    for (const auto& iter : task_list) {
        if (iter.second->status == ETaskStatus::TS_Creating) continue;
        dbcMonitor::domMonitorData dmData;
        dmData.domain_name = iter.first;
        dmData.delay = 10;
        dmData.version = dbcversion();
        if (!VmClient::instance().GetDomainMonitorData(iter.first, dmData)) {
            TASK_LOG_ERROR(iter.first, "get domain monitor data error");
            continue;
        }
        const auto lastData = m_monitor_datas.find(iter.first);
        if (lastData == m_monitor_datas.end()) {
            m_monitor_datas[iter.first] = dmData;
        } else {
            // 计算CPU使用率、磁盘读写速度和网络收发速度
            dmData.calculatorUsageAndSpeed(lastData->second);
            m_monitor_datas[iter.first] = dmData;
        }
    }
    for (auto iter = m_monitor_datas.begin(); iter != m_monitor_datas.end();) {
        if (task_list.find(iter->first) == task_list.end()) {
            TASK_LOG_INFO(iter->first, "task not existed, so monitor data will be deleted later");
            // task被删除了
            iter = m_monitor_datas.erase(iter);
        } else {
            iter++;
        }
    }
}

void node_monitor_service::send_monitor_data() {
    for (const auto& iter : m_monitor_datas) {
        // TASK_LOG_INFO(iter.first, iter.second.toJsonString());
        for (const auto& server : m_monitor_servers) {
            zabbixSender zs(server.ip, server.port);
            if (zs.is_server_want_monitor_data(iter.first)) {
                if (!zs.sendJsonData(iter.second.toZabbixString(iter.first))) {
                    TASK_LOG_ERROR(iter.first, "send zabbix data error");
                } else {
                    TASK_LOG_INFO(iter.first, "send zabbix data success");
                }
            } else {
                LOG_ERROR << "server: " << server.ip << " does not need monitor data of vm: " << iter.first;
            }
        }
    }
}
