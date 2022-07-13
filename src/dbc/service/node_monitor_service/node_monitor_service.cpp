#include "node_monitor_service.h"
#include "db/db_types/monitors_db_types.h"
#include "zabbixSender.h"
#include "task/vm/vm_client.h"
#include "common/version.h"
#include "config/conf_manager.h"
#include "log/log.h"
#include "network/protocol/thrift_binary.h"
#include "util/system_info.h"
#include "server/server.h"
#include "task/HttpDBCChainClient.h"
#include "task/detail/gpu/TaskGpuManager.h"

#define MAX_MONITOR_QUEUE_SIZE 1024

node_monitor_service::~node_monitor_service() {

}

ERRCODE node_monitor_service::init() {
    service_module::init();

    if (Server::NodeType == NODE_TYPE::ComputeNode) {
        std::string default_monitor = ConfManager::instance().GetDbcMonitorServer();
        std::vector<std::string> vecSplit = util::split(default_monitor, ":");
        if (vecSplit.size() != 2) {
            LOG_ERROR << "parse dbc monitor server error";
            return E_DEFAULT;
        }
        m_dbc_monitor_server.host = vecSplit[0];
        m_dbc_monitor_server.port = vecSplit[1];

        default_monitor = ConfManager::instance().GetMinerMonitorServer();
        if (!default_monitor.empty()) {
            vecSplit = util::split(default_monitor, ":");
            if (vecSplit.size() == 2) {
                m_miner_monitor_server.host = vecSplit[0];
                m_miner_monitor_server.port = vecSplit[1];
            }
        }

        m_io_service_pool = std::make_shared<network::io_service_pool>();
        if (ERR_SUCCESS != m_io_service_pool->init(1)) {
            LOG_ERROR << "init monitor io service failed";
            return E_DEFAULT;
        }

        if (ERR_SUCCESS != m_io_service_pool->start()) {
            LOG_ERROR << "monitor io service run failed";
            return E_DEFAULT;
        }

        if (ERR_SUCCESS != init_db()) {
            LOG_ERROR << "init_db error";
            return E_DEFAULT;
        }

        load_wallet_monitor_from_db();
    }

    return ERR_SUCCESS;
}

void node_monitor_service::exit() {
	service_module::exit();

    LOG_INFO << "wait for monitor socket close";
    for (const auto& iter : m_senders) {
        int count = 0;
        while (!iter->overed() && count++ < 3) {
            sleep(3);
        }
        if (!iter->overed()) iter->stop();
    }

    if (m_io_service_pool) {
        m_io_service_pool->stop();
        m_io_service_pool->exit();
    }
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
        return FResult(ERR_ERROR, "additional parse failed");
    }
    std::vector<monitor_server> servers;
    if (doc.HasMember("servers")) {
        const rapidjson::Value& v_servers = doc["servers"];
        if (v_servers.IsArray()) {
            if (v_servers.Size() > 2) {
                return FResult(ERR_ERROR, "can not enter more than two servers");
            }
            for (rapidjson::SizeType i = 0; i < v_servers.Size(); i++) {
                const rapidjson::Value& v_item = v_servers[i];
                if (v_item.IsString()) {
                    std::string str = v_item.GetString();
                    std::vector<std::string> vecSplit = util::split(str, ":");
                    if (vecSplit.size() != 2) {
                        return FResult(ERR_ERROR, "server not match format ip:port");
                    }
                    servers.push_back({vecSplit[0], vecSplit[1]});
                    LOG_INFO << "set monitor server host:" << vecSplit[0] << ", port:" << vecSplit[1];
                }
            }
            m_wallet_monitors[wallet] = servers;
        } else {
            return FResult(ERR_ERROR, "servers must be an array");
        }
    }
    return FResultOk;
}

void node_monitor_service::init_timer() {
    if (Server::NodeType == NODE_TYPE::ComputeNode) {
        // 30s
        add_timer(MONITOR_DATA_SENDER_TASK_TIMER, 30 * 1000, 30 * 1000, ULLONG_MAX, "",
            CALLBACK_1(node_monitor_service::on_monitor_data_sender_task_timer, this));

        // 3min
        add_timer(UPDATE_CUR_RENTER_WALLET_TIMER, 180 * 1000, 180 * 1000, ULLONG_MAX, "",
            CALLBACK_1(node_monitor_service::on_update_cur_renter_wallet_timer, this));

        // 1min
        add_timer(LIBVIRT_AUTO_RECONNECT_TIMER, 60 * 1000, 60 * 1000, ULLONG_MAX, "",
            CALLBACK_1(node_monitor_service::on_libvirt_auto_reconnect_timer, this));
    }
}

void node_monitor_service::init_invoker() {

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
            network::binary_protocol proto(wallet_monitors_buf.get());
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

    auto renttasks = WalletRentTaskMgr::instance().getAllWalletRentTasks();
    for (const auto& rentlist : renttasks) {
        auto monitor_servers = m_wallet_monitors.find(rentlist.first);
        if (rentlist.first == m_cur_renter_wallet) {
            hmData.vmCount = rentlist.second->getTaskIds().size();
        }

        auto ids = rentlist.second->getTaskIds();
        for (const auto& task_id : ids) {
            if (task_id.find("vm_check_") != std::string::npos) continue;
            auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
            if (!taskinfo) continue;
            if (rentlist.first == m_cur_renter_wallet) {
                if (taskinfo && taskinfo->getTaskStatus() == TaskStatus::TS_Task_Running) {
                    auto gpus = TaskGpuMgr::instance().getTaskGpus(task_id);
                    hmData.gpuUsed += gpus.size();
                    hmData.vmRunning++;
                }
            }
            if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Creating ||
                taskinfo->getTaskStatus() == TaskStatus::TS_CreateTaskError)
                continue;

            // get monitor data of vm
            dbcMonitor::domMonitorData dmData;
            dmData.domainName = task_id;
            dmData.delay = 30;
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

            if (rentlist.first == m_cur_renter_wallet && taskinfo 
                && taskinfo->getTaskStatus() == TaskStatus::TS_Task_Running) {
                for (const auto& gpuStat : dmData.gpuStats) {
                    hmData.gpuStats[gpuStat.first] = gpuStat.second;
                }
            }

            for (const auto & netStat : dmData.netStats) {
                hmData.rxFlow += netStat.second.rx_bytes;
                hmData.txFlow += netStat.second.tx_bytes;
            }

            // send monitor data
            if (monitor_servers != m_wallet_monitors.end()) {
                for (const auto& server : monitor_servers->second) {
                    // send_monitor_data(dmData, server);
                    add_monitor_send_queue(server, dmData.domainName, dmData.toZabbixString());
                }
            }
            // send_monitor_data(dmData, m_dbc_monitor_server);
            // add_monitor_send_queue(m_dbc_monitor_server, dmData.domainName, dmData.toZabbixString());
        }
    }
    
    auto task_list = TaskInfoMgr::instance().getAllTaskInfos();
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
    hmData.delay = 30;
    hmData.gpuCount = SystemInfo::instance().GetGpuInfo().size();
    hmData.cpuUsage = SystemInfo::instance().GetCpuUsage() * 100;
    hmData.memTotal = SystemInfo::instance().GetMemInfo().total;
    hmData.memFree = SystemInfo::instance().GetMemInfo().free;
    hmData.memUsage = SystemInfo::instance().GetMemInfo().usage * 100;
    // hmData.flow 各虚拟机的流量之和
	disk_info _disk_info;
	SystemInfo::instance().GetDiskInfo("/data", _disk_info);

    hmData.diskTotal = _disk_info.total;
    hmData.diskFree = _disk_info.available;
    hmData.diskUsage = _disk_info.usage * 100;
    hmData.diskMountStatus = _disk_info.data_mount_status;
    hmData.loadAverage = SystemInfo::instance().loadaverage();
    // hmData.packetLossRate
    hmData.version = dbcversion();

    if (!m_cur_renter_wallet.empty()) {
        auto monitor_servers = m_wallet_monitors.find(m_cur_renter_wallet);
        if (monitor_servers != m_wallet_monitors.end()) {
            for (const auto& server : monitor_servers->second) {
                // send_monitor_data(hmData, server);
                add_monitor_send_queue(server, hmData.nodeId, hmData.toZabbixString());
            }
        }
    }
    // send_monitor_data(hmData, m_dbc_monitor_server);
    // send_monitor_data(hmData, m_miner_monitor_server);
    add_monitor_send_queue(m_dbc_monitor_server, hmData.nodeId, hmData.toZabbixString());
    add_monitor_send_queue(m_miner_monitor_server, hmData.nodeId, hmData.toZabbixString());
}

void node_monitor_service::on_update_cur_renter_wallet_timer(const std::shared_ptr<core_timer>& timer) {
    std::string cur_renter_wallet;
    auto wallets = WalletRentTaskMgr::instance().getAllWalletRentTasks();
    for (auto& it : wallets) {
        int64_t rent_end = HttpDBCChainClient::instance().request_rent_end(
            ConfManager::instance().GetNodeId(), it.first);
        if (rent_end > 0) {
            cur_renter_wallet = it.first;
            break;
        }
    }
    m_cur_renter_wallet = cur_renter_wallet;
}

void node_monitor_service::on_libvirt_auto_reconnect_timer(const std::shared_ptr<core_timer>& timer) {
    if (!VmClient::instance().IsConnectAlive()) {
        LOG_INFO << "Detected disconnected to libvirt";
        FResult fret = VmClient::instance().OpenConnect();
        if (fret.errcode == ERR_SUCCESS)
            LOG_INFO << "reconnect successful";
        else
            LOG_ERROR << "reconnect failed, will automatically retry later";
    }
}

void node_monitor_service::add_monitor_send_queue(const monitor_server& server, const std::string& host, const std::string& json) {
    if (server.host.empty() || server.port.empty() || host.empty() || json.empty()) return;

    for (auto iter = m_senders.begin(); iter != m_senders.end();) {
        if ((*iter)->overed()) {
            iter = m_senders.erase(iter);
        } else {
            iter++;
        }
    }

    if (m_monitor_send_queue.size() > MAX_MONITOR_QUEUE_SIZE) {
        LOG_INFO << "monitor send queue is full, the oldest data will be deleted";
        m_monitor_send_queue.pop();
    }
    m_monitor_send_queue.push({server, host, json});

    while (!m_monitor_send_queue.empty() && m_senders.size() < 10) {
        monitor_param param = m_monitor_send_queue.front();
        m_monitor_send_queue.pop();
        std::shared_ptr<zabbixSender> zs =
            std::make_shared<zabbixSender>(*m_io_service_pool->get_io_service().get(),
                param.host, param.json);
        zs->start(param.server.host, param.server.port);
        m_senders.push_back(std::move(zs));
    }
}

void node_monitor_service::send_monitor_data(const dbcMonitor::domMonitorData& dmData, const monitor_server& server) const {
    // if (server.host.empty() || server.port.empty()) return;
    // // TASK_LOG_INFO(dmData.domainName, dmData.toJsonString());
    // zabbixSender zs(server.host, server.port);
    // if (zs.is_server_want_monitor_data(dmData.domainName)) {
    //     if (!zs.sendJsonData(dmData.toZabbixString())) {
    //         LOG_ERROR << "send monitor data of task(" << dmData.domainName << ") to server(" << server.host << ") error";
    //         // TASK_LOG_ERROR(dmData.domainName, "send monitor data error");
    //     } else {
    //         LOG_INFO << "send monitor data of task(" << dmData.domainName << ") to server(" << server.host << ") success";
    //         // TASK_LOG_INFO(dmData.domainName, "send monitor data success");
    //     }
    // } else {
    //     LOG_ERROR << "server: " << server.host << " does not need monitor data of vm: " << dmData.domainName;
    // }
}

void node_monitor_service::send_monitor_data(const dbcMonitor::hostMonitorData& hmData, const monitor_server& server) const {
    // if (server.host.empty() || server.port.empty()) return;
    // // LOG_INFO << "machine monitor data:" << hmData.toJsonString();
    // zabbixSender zs(server.host, server.port);
    // if (zs.is_server_want_monitor_data(hmData.nodeId)) {
    //     if (!zs.sendJsonData(hmData.toZabbixString())) {
    //         LOG_ERROR << "send host monitor data to server(" << server.host << ") error";
    //     } else {
    //         LOG_INFO << "send host monitor data to server(" << server.host << ") success";
    //     }
    // } else {
    //     LOG_ERROR << "server: " << server.host << " does not need host monitor data";
    // }
}
