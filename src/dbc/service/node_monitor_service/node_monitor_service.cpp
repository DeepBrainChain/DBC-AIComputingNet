#include "node_monitor_service.h"
#include "monitors_db_types.h"
#include "zabbixSender.h"
#include "service/task/TaskInfoManager.h"
#include "service/task/WalletRentTaskManager.h"
#include "service/task/vm/vm_client.h"
#include "common/version.h"
#include "config/conf_manager.h"
#include "log/log.h"
#include "network/protocol/thrift_binary.h"
#include "util/system_info.h"
#include "server/server.h"

node_monitor_service::~node_monitor_service() {
    if (Server::NodeType == DBC_NODE_TYPE::DBC_COMPUTE_NODE) {
        remove_timer(m_monitor_data_sender_task_timer_id);
        remove_timer(m_update_cur_renter_wallet_timer_id);
    }
}

int32_t node_monitor_service::init(bpo::variables_map &options) {
    if (Server::NodeType == DBC_NODE_TYPE::DBC_COMPUTE_NODE) {
        std::string default_monitor = ConfManager::instance().GetDbcMonitorServer();
        std::vector<std::string> vecSplit = util::split(default_monitor, ":");
        if (vecSplit.size() != 2) {
            LOG_ERROR << "parse dbc monitor server error";
            return E_DEFAULT;
        }
        m_dbc_monitor_server.host = vecSplit[0];
        m_dbc_monitor_server.port = vecSplit[1];

        if (ERR_SUCCESS != init_db()) {
            LOG_ERROR << "init_db error";
            return E_DEFAULT;
        }

        load_wallet_monitor_from_db();
    }

    service_module::init();

    return ERR_SUCCESS;
}

void node_monitor_service::listMonitorServer(const std::string& wallet, std::vector<monitor_server>& servers) const {
    auto monitor_servers = m_wallet_monitors.find(wallet);
    if (monitor_servers != m_wallet_monitors.end()) {
        for (const auto& server : monitor_servers->second) {
            servers.push_back(server);
        }
    }
}

FResult node_monitor_service::setMonitorServer(const std::string& wallet, const std::string &additional) {
    rapidjson::Document doc;
    doc.Parse(additional.c_str());
    if (!doc.IsObject()) {
        return {E_DEFAULT, "additional parse failed"};
    }
    std::vector<monitor_server> servers;
    if (doc.HasMember("servers")) {
        const rapidjson::Value& v_servers = doc["servers"];
        if (v_servers.IsArray()) {
            if (v_servers.Size() > 2) {
                return {E_DEFAULT, "can not enter more than two servers"};
            }
            for (rapidjson::SizeType i = 0; i < v_servers.Size(); i++) {
                const rapidjson::Value& v_item = v_servers[i];
                if (v_item.IsString()) {
                    std::string str = v_item.GetString();
                    std::vector<std::string> vecSplit = util::split(str, ":");
                    if (vecSplit.size() != 2) {
                        return {E_DEFAULT, "server not match format ip:port"};
                    }
                    servers.push_back({vecSplit[0], vecSplit[1]});
                    LOG_INFO << "set monitor server host:" << vecSplit[0] << ", port:" << vecSplit[1];
                }
            }
            m_wallet_monitors[wallet] = servers;
        } else {
            return {E_DEFAULT, "servers must be an array"};
        }
    }
    return FResultOK;
}

void node_monitor_service::init_timer() {
    if (Server::NodeType == DBC_NODE_TYPE::DBC_COMPUTE_NODE) {
        // 1min
        // 10s
        m_timer_invokers[MONITOR_DATA_SENDER_TASK_TIMER] = std::bind(&node_monitor_service::on_monitor_data_sender_task_timer, this, std::placeholders::_1);
        m_monitor_data_sender_task_timer_id = this->add_timer(MONITOR_DATA_SENDER_TASK_TIMER, 10 * 1000, ULLONG_MAX, "");

        // 3min
        m_timer_invokers[UPDATE_CUR_RENTER_WALLET_TIMER] = std::bind(&node_monitor_service::on_update_cur_renter_wallet_timer, this, std::placeholders::_1);
        m_update_cur_renter_wallet_timer_id = this->add_timer(UPDATE_CUR_RENTER_WALLET_TIMER, 180 * 1000, ULLONG_MAX, "");
    }
}

void node_monitor_service::init_invoker() {

}

void node_monitor_service::init_subscription() {

}

int32_t node_monitor_service::init_db() {
    leveldb::DB *db = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;
    try {
        bfs::path monitor_db_path = EnvManager::instance().get_db_path();
        if (false == bfs::exists(monitor_db_path)) {
            LOG_DEBUG << "db directory path does not exist and create db directory";
            bfs::create_directory(monitor_db_path);
        }

        if (false == bfs::is_directory(monitor_db_path)) {
            LOG_ERROR << "db directory path does not exist and exit";
            return E_DEFAULT;
        }

        monitor_db_path /= bfs::path("monitor.db");
        leveldb::Status status = leveldb::DB::Open(options, monitor_db_path.generic_string(), &db);
        if (false == status.ok()) {
            LOG_ERROR << "node monitor service init monitor db error: " << status.ToString();
            return E_DEFAULT;
        }

        m_wallet_monitors_db.reset(db);
    }
    catch (const std::exception &e) {
        LOG_ERROR << "create monitor db error: " << e.what();
        return E_DEFAULT;
    }
    catch (const boost::exception &e) {
        LOG_ERROR << "create monitor db error" << diagnostic_information(e);
        return E_DEFAULT;
    }

    return ERR_SUCCESS;
}

int32_t node_monitor_service::load_wallet_monitor_from_db() {
    m_wallet_monitors.clear();

    try {
        // ip_validator ip_vdr;
        // port_validator port_vdr;

        std::unique_ptr<leveldb::Iterator> it;
        it.reset(m_wallet_monitors_db->NewIterator(leveldb::ReadOptions()));
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            std::shared_ptr<dbc::db_wallet_monitors> db_wallet_monitor = std::make_shared<dbc::db_wallet_monitors>();

            std::shared_ptr<byte_buf> wallet_monitors_buf = std::make_shared<byte_buf>();
            wallet_monitors_buf->write_to_byte_buf(it->value().data(), (uint32_t) it->value().size());
            dbc::network::binary_protocol proto(wallet_monitors_buf.get());
            db_wallet_monitor->read(&proto);             //may exception

            if (db_wallet_monitor->wallet.empty()) {
                LOG_ERROR << "load wallet monitor error, node_id is empty";
                continue;
            }

            std::vector<monitor_server> servers;
            for (const auto& monitor : db_wallet_monitor->monitors) {
                servers.push_back({monitor.host, monitor.port});
            }

            // variable_value val_ip(db_wallet_monitor->ip, false);
            // if (!ip_vdr.validate(val_ip)) {
            //     LOG_ERROR << "load wallet monitor error, ip is invalid: " << db_wallet_monitor->ip;
            //     continue;
            // }

            // variable_value val_port(std::to_string((unsigned int) (uint16_t) db_wallet_monitor->port), false);
            // if (!port_vdr.validate(val_port)) {
            //     LOG_ERROR << "load wallet monitor error, port is invalid: " << (uint16_t) db_wallet_monitor->port;
            //     continue;
            // }

            m_wallet_monitors[db_wallet_monitor->wallet] = servers;
        }
    }
    catch (std::exception& e) {
        LOG_ERROR << "load wallet monitors from db exception: " << e.what();
        return E_DEFAULT;
    }

    return ERR_SUCCESS;
}

void node_monitor_service::on_monitor_data_sender_task_timer(const std::shared_ptr<core_timer>& timer) {
    // 宿主机的监控信息
    dbcMonitor::hostMonitorData hmData;
    hmData.gpuCount = hmData.gpuUsed = hmData.vmCount = hmData.vmRunning = 0;
    hmData.rxFlow = hmData.txFlow = 0;

    auto renttasks = WalletRentTaskMgr::instance().getRentTasks();
    for (const auto& rentlist : renttasks) {
        auto monitor_servers = m_wallet_monitors.find(rentlist.first);
        if (rentlist.first == m_cur_renter_wallet) {
            hmData.vmCount = rentlist.second->task_ids.size();
        }
        for (const auto& task_id : rentlist.second->task_ids) {
            if (task_id.find("vm_check_") != std::string::npos) continue;
            auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
            if (rentlist.first == m_cur_renter_wallet) {
                if (taskinfo && taskinfo->status == ETaskStatus::TS_Running) {
                    std::shared_ptr<TaskResource> task_resource = TaskResourceMgr::instance().getTaskResource(task_id);
                    hmData.gpuUsed += task_resource->gpus.size();
                    hmData.vmRunning++;
                }
            }
            if (!taskinfo || taskinfo->status == ETaskStatus::TS_Creating) continue;

            // get monitor data of vm
            dbcMonitor::domMonitorData dmData;
            dmData.domainName = task_id;
            dmData.delay = 10;
            dmData.version = dbcversion();
            if (!VmClient::instance().GetDomainMonitorData(task_id, dmData)) {
                TASK_LOG_ERROR(task_id, "get domain monitor data error");
                continue;
            }
            const auto lastData = m_monitor_datas.find(task_id);
            if (lastData == m_monitor_datas.end()) {
                m_monitor_datas[task_id] = dmData;
            } else {
                // 计算CPU使用率、磁盘读写速度和网络收发速度
                dmData.calculatorUsageAndSpeed(lastData->second);
                m_monitor_datas[task_id] = dmData;
            }

            for (const auto & netStat : dmData.netStats) {
                hmData.rxFlow += netStat.second.rx_bytes;
                hmData.txFlow += netStat.second.tx_bytes;
            }

            // send monitor data
            if (monitor_servers != m_wallet_monitors.end()) {
                for (const auto& server : monitor_servers->second) {
                    send_monitor_data(dmData, server);
                }
            }
            send_monitor_data(dmData, m_dbc_monitor_server);
        }
    }
    const std::map<std::string, std::shared_ptr<dbc::TaskInfo>> task_list = TaskInfoMgr::instance().getTasks();
    for (auto iter = m_monitor_datas.begin(); iter != m_monitor_datas.end();) {
        if (task_list.find(iter->first) == task_list.end()) {
            TASK_LOG_INFO(iter->first, "task not existed, so monitor data will be deleted later");
            // task被删除了
            iter = m_monitor_datas.erase(iter);
        } else {
            iter++;
        }
    }

    hmData.nodeId = ConfManager::instance().GetNodeId();
    hmData.delay = 10;
    hmData.gpuCount = SystemInfo::instance().GetGpuInfo().size();
    hmData.cpuUsage = SystemInfo::instance().GetCpuUsage();
    hmData.memTotal = SystemInfo::instance().GetMemInfo().total;
    hmData.memFree = SystemInfo::instance().GetMemInfo().free;
    hmData.memUsage = SystemInfo::instance().GetMemInfo().usage;
    // hmData.flow 各虚拟机的流量之和
    hmData.diskTotal = SystemInfo::instance().GetDiskInfo().total;
    hmData.diskFree = SystemInfo::instance().GetDiskInfo().available;
    hmData.diskUsage = SystemInfo::instance().GetDiskInfo().usage;
    hmData.loadAverage = SystemInfo::instance().loadaverage();
    // hmData.packetLossRate
    hmData.version = dbcversion();

    if (!m_cur_renter_wallet.empty()) {
        auto monitor_servers = m_wallet_monitors.find(m_cur_renter_wallet);
        if (monitor_servers != m_wallet_monitors.end()) {
            for (const auto& server : monitor_servers->second) {
                send_monitor_data(hmData, server);
            }
        }
    }
    send_monitor_data(hmData, m_dbc_monitor_server);
}

void node_monitor_service::on_update_cur_renter_wallet_timer(const std::shared_ptr<core_timer>& timer) {
    std::string cur_renter_wallet;
    std::vector<std::string> wallets = WalletRentTaskMgr::instance().getAllWallet();
    for (auto& it : wallets) {
        int64_t rent_end = m_httpclient.request_rent_end(it);
        if (rent_end > 0) {
            cur_renter_wallet = it;
            break;
        }
    }
    m_cur_renter_wallet = cur_renter_wallet;
}

void node_monitor_service::send_monitor_data(const dbcMonitor::domMonitorData& dmData, const monitor_server& server) const {
    // TASK_LOG_INFO(dmData.domainName, dmData.toJsonString());
    zabbixSender zs(server.host, server.port);
    if (zs.is_server_want_monitor_data(dmData.domainName)) {
        if (!zs.sendJsonData(dmData.toZabbixString())) {
            LOG_ERROR << "send monitor data of task(" << dmData.domainName << ") to server(" << server.host << ") error";
            // TASK_LOG_ERROR(dmData.domainName, "send monitor data error");
        } else {
            LOG_INFO << "send monitor data of task(" << dmData.domainName << ") to server(" << server.host << ") success";
            // TASK_LOG_INFO(dmData.domainName, "send monitor data success");
        }
    } else {
        LOG_ERROR << "server: " << server.host << " does not need monitor data of vm: " << dmData.domainName;
    }
}

void node_monitor_service::send_monitor_data(const dbcMonitor::hostMonitorData& hmData, const monitor_server& server) const {
    LOG_INFO << "machine monitor data:" << hmData.toJsonString();
    zabbixSender zs(server.host, server.port);
    if (zs.is_server_want_monitor_data(hmData.nodeId)) {
        if (!zs.sendJsonData(hmData.toZabbixString())) {
            LOG_ERROR << "send host monitor data to server(" << server.host << ") error";
        } else {
            LOG_INFO << "send host monitor data to server(" << server.host << ") success";
        }
    } else {
        LOG_ERROR << "server: " << server.host << " does not need host monitor data";
    }
}
