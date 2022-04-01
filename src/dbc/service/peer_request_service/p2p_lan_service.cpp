#include "p2p_lan_service.h"
#include "server/server.h"
#include "config/conf_manager.h"
#include "util/system_info.h"

// multicast message type
#define REQUEST_MACHINE_INFO "machine info"
#define REQUEST_MACHINE_EXIT "machine exit"

bool lan_machine_info::validate() const {
    if (machine_id.empty() || local_address.empty() || local_port == 0) return false;
    if (net_type.empty() || net_flag == 0) return false;
    if (node_type != NODE_TYPE::ComputeNode && node_type != NODE_TYPE::ClientNode && node_type != NODE_TYPE::SeedNode) return false;
    if (cur_time == 0) return false;
    ip_validator ip_vdr;
    variable_value val_ip(local_address, false);
    if (!ip_vdr.validate(val_ip)) return false;
    port_validator port_vdr;
    variable_value val_port(std::to_string((uint16_t)local_port), false);
    if (!port_vdr.validate(val_port)) return false;
    return true;
}

std::string lan_machine_info::to_request_json() const {
    rapidjson::StringBuffer strBuf;
    rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
    write.SetMaxDecimalPlaces(2);
    write.StartObject();
    write.Key("request");
    write.String(REQUEST_MACHINE_INFO);
    write.Key("data");
    write.StartObject();
    write.Key("machine_id");
    write.String(machine_id.c_str());
    write.Key("net_type");
    write.String(net_type.c_str());
    write.Key("net_flag");
    write.Int(net_flag);
    // write.Key("local_address");
    // write.String(local_address.c_str());
    write.Key("local_port");
    write.Uint(local_port);
    write.Key("node_type");
    write.Int((int32_t) node_type);
    // struct timespec ts;
    // clock_gettime(CLOCK_REALTIME, &ts);
    // write.Key("cur_time");
    // write.Int64(ts.tv_sec);
    write.EndObject();
    write.EndObject();
    return strBuf.GetString();
}

ERRCODE p2p_lan_service::init() {
    service_module::init();

    try {
        m_io_service_pool = std::make_shared<network::io_service_pool>();
        ERRCODE err = m_io_service_pool->init(1);
        if (ERR_SUCCESS != err) {
            LOG_ERROR << "init multicast io service failed";
            return err;
        }

        m_receiver = std::make_shared<multicast_receiver>(*m_io_service_pool->get_io_service().get(),
            boost::asio::ip::make_address(ConfManager::instance().GetNetListenIp()),
            boost::asio::ip::make_address(ConfManager::instance().GetMulticastAddress()),
            ConfManager::instance().GetMulticastPort());
        err = m_receiver->start();
        if (ERR_SUCCESS != err) {
            LOG_ERROR << "init multicast receiver failed";
            return err;
        }

        m_sender = std::make_shared<multicast_sender>(*m_io_service_pool->get_io_service().get(),
            boost::asio::ip::make_address(ConfManager::instance().GetMulticastAddress()),
            ConfManager::instance().GetMulticastPort());

        err = m_io_service_pool->start();
        if (ERR_SUCCESS != err) {
            LOG_ERROR << "multicast io service run failed";
            return err;
        }

        init_local_machine_info();
    }
    catch (std::exception& e)
    {
        LOG_ERROR << "init multicast receiver Exception: " << e.what();
        return -1;
    }

    return ERR_SUCCESS;
}

void p2p_lan_service::exit() {
	service_module::exit();
    m_receiver->stop();
    send_machine_exit_request();
    sleep(2);
    // m_sender->stop();
    m_io_service_pool->stop();
    m_io_service_pool->exit();
}

const std::map<std::string, lan_machine_info>& p2p_lan_service::get_lan_nodes() const {
    RwMutex::ReadLock rlock(m_mtx);
    return m_lan_nodes;
}

void p2p_lan_service::add_lan_node(const lan_machine_info& lm_info) {
    if (!lm_info.validate()) return;
    if (lm_info.machine_id == ConfManager::instance().GetNodeId()) return;
    LOG_INFO << "update machine info of node " << lm_info.machine_id;
    RwMutex::WriteLock wlock(m_mtx);
    m_lan_nodes[lm_info.machine_id] = lm_info;
}

void p2p_lan_service::delete_lan_node(const std::string& machine_id) {
    if (machine_id.empty()) return ;
    auto iter = m_lan_nodes.find(machine_id);
    if (iter == m_lan_nodes.end()) return;
    LOG_INFO << "delete machine info of node " << machine_id;
    RwMutex::WriteLock wlock(m_mtx);
    m_lan_nodes.erase(iter);
}

void p2p_lan_service::on_multicast_receive(const std::string& data, const std::string& addr) {
    rapidjson::Document doc;
    doc.Parse(data.c_str());
    if (!doc.IsObject()) {
        LOG_ERROR << "parse multicast message failed";
        return;
    }

    std::string request_type;
    JSON_PARSE_STRING(doc, "request", request_type);

    if (request_type == REQUEST_MACHINE_INFO) {
        lan_machine_info lm_info;
        const rapidjson::Value& v_data = doc["data"];
        if (!v_data.IsObject()) return;
        JSON_PARSE_STRING(v_data, "machine_id", lm_info.machine_id);
        JSON_PARSE_STRING(v_data, "net_type", lm_info.net_type);
        JSON_PARSE_INT(v_data, "net_flag", lm_info.net_flag);
        lm_info.local_address = addr;
        JSON_PARSE_UINT(v_data, "local_port", lm_info.local_port);
        int32_t tmp_node_type = 0;
        JSON_PARSE_INT(v_data, "node_type", tmp_node_type);
        lm_info.node_type = (NODE_TYPE) tmp_node_type;
        // JSON_PARSE_INT64(v_data, "cur_time", lm_info.cur_time);
        lm_info.cur_time = time(NULL);
        add_lan_node(lm_info);
    } else if (request_type == REQUEST_MACHINE_EXIT) {
        std::string machine_id;
        const rapidjson::Value& v_data = doc["data"];
        if (!v_data.IsObject()) return;
        JSON_PARSE_STRING(v_data, "machine_id", machine_id);
        delete_lan_node(machine_id);
    }
}

void p2p_lan_service::init_timer() {
    // 30second
    add_timer(AI_MULTICAST_MACHINE_TIMER, 5 * 1000, 30 * 1000, ULLONG_MAX, "",
        std::bind(&p2p_lan_service::on_multicast_machine_task_timer, this, std::placeholders::_1));
    // 1min
    add_timer(AI_MULTICAST_NETWORK_TIMER, 60 * 1000, 60 * 1000, ULLONG_MAX, "",
        std::bind(&p2p_lan_service::on_multicast_network_task_timer, this, std::placeholders::_1));
}

void p2p_lan_service::init_invoker() {

}

void p2p_lan_service::init_local_machine_info() {
    m_local_machine_info.machine_id = ConfManager::instance().GetNodeId();
    m_local_machine_info.net_type = ConfManager::instance().GetNetType();
    m_local_machine_info.net_flag = ConfManager::instance().GetNetFlag();
    m_local_machine_info.local_address = SystemInfo::instance().GetDefaultRouteIp();
    m_local_machine_info.local_port = ConfManager::instance().GetNetListenPort();
    m_local_machine_info.node_type = Server::NodeType;
    m_local_machine_message = m_local_machine_info.to_request_json();
}

void p2p_lan_service::on_multicast_machine_task_timer(const std::shared_ptr<core_timer>& timer) {
    if (!m_local_machine_message.empty() && m_sender) {
        m_sender->send(m_local_machine_message);
    }
    time_t cur_time = time(NULL);
    for (auto iter = m_lan_nodes.begin(); iter != m_lan_nodes.end();) {
        // 如果5min没有收到任何REQUEST_MACHINE_INFO消息，则判断为推出
        if (difftime(cur_time, iter->second.cur_time) > 300) {
            LOG_INFO << "No heartbeat received for a long time, so " << iter->first << " exit";
            RwMutex::WriteLock wlock(m_mtx);
            iter = m_lan_nodes.erase(iter);
        } else {
            iter++;
        }
    }
}

void p2p_lan_service::on_multicast_network_task_timer(const std::shared_ptr<core_timer>& timer) {
    //
}

void p2p_lan_service::send_machine_exit_request() const {
    if (m_sender) {
        rapidjson::StringBuffer strBuf;
        rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
        write.SetMaxDecimalPlaces(2);
        write.StartObject();
        write.Key("request");
        write.String(REQUEST_MACHINE_EXIT);
        write.Key("data");
        write.StartObject();
        write.Key("machine_id");
        write.String(m_local_machine_info.machine_id.c_str());
        write.EndObject();
        write.EndObject();
        m_sender->send(strBuf.GetString());
    }
}
