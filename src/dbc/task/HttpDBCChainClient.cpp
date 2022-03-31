#include "HttpDBCChainClient.h"
#include "log/log.h"
#include "config/conf_manager.h"

HttpDBCChainClient::HttpDBCChainClient() {

}

HttpDBCChainClient::~HttpDBCChainClient() {

}

void HttpDBCChainClient::init(const std::vector<std::string>& dbc_chain_addrs) {
    for (int i = 0; i < dbc_chain_addrs.size(); i++) {
        std::vector<std::string> addr = util::split(dbc_chain_addrs[i], ":");
        if (addr.empty()) continue;

        network::net_address net_addr;
        if (addr.size() > 0) {
            net_addr.set_ip(addr[0]);
        }
        if (addr.size() > 1) {
            net_addr.set_port((uint16_t) atoi(addr[1].c_str()));
        }
        else {
            net_addr.set_port(443);
        }
         
        httplib::SSLClient cli(net_addr.get_ip(), net_addr.get_port());
        cli.set_timeout_sec(5);
        cli.set_read_timeout(10, 0);
        if (cli.is_valid()) {
            struct timeval tv1{};
            gettimeofday(&tv1, 0);

            // cur block
            std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"chain_getBlock", "params": []})";
            std::shared_ptr<httplib::Response> resp = cli.Post("/", str_send, "application/json");
            if (resp == nullptr) {
                m_addrs[0xFF + i] = net_addr;
                continue;
            }

            struct timeval tv2 {};
            gettimeofday(&tv2, 0);

            int64_t msec = (tv2.tv_sec * 1000 + tv2.tv_usec / 1000) - (tv1.tv_sec * 1000 + tv1.tv_usec / 1000);
            m_addrs[msec] = net_addr;
        }
        else {
            m_addrs[0xFFFF + i] = net_addr;
        }
    }
}

int64_t HttpDBCChainClient::request_cur_block() {
    int64_t cur_block = 0;

    for (auto& it : m_addrs) {
        httplib::SSLClient cli(it.second.get_ip(), it.second.get_port());
        cli.set_timeout_sec(5);
        cli.set_read_timeout(10, 0);

        std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"chain_getBlock", "params": []})";
        std::shared_ptr<httplib::Response> resp = cli.Post("/", str_send, "application/json");
        if (resp != nullptr) {
            rapidjson::Document doc;
            rapidjson::ParseResult ok = doc.Parse(resp->body.c_str());
            if (!ok) continue;
            
            if (!doc.HasMember("result")) break;
            const rapidjson::Value& v_result = doc["result"];
            if (!v_result.IsObject()) break;

            if (!v_result.HasMember("block")) break;
            const rapidjson::Value& v_block = v_result["block"];
            if (!v_block.IsObject()) break;

            if (!v_block.HasMember("header")) break;
            const rapidjson::Value& v_header = v_block["header"];
            if (!v_header.IsObject()) break;

            if (!v_header.HasMember("number")) break;
            const rapidjson::Value& v_number = v_header["number"];
            if (!v_number.IsString()) break;

            char* p;
            try {
                cur_block = strtol(v_number.GetString(), &p, 16);
            }
            catch (...) {
                cur_block = 0;
            }

            break;
        }
    }
    
    return cur_block;
}

MACHINE_STATUS HttpDBCChainClient::request_machine_status(const std::string& node_id) {
    std::string machine_status;

    for (auto& it : m_addrs) {
        httplib::SSLClient cli(it.second.get_ip(), it.second.get_port());
        cli.set_timeout_sec(5);
        cli.set_read_timeout(10, 0);

        std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"onlineProfile_getMachineInfo", "params": [")"
            + node_id + R"("]})";
        std::shared_ptr<httplib::Response> resp = cli.Post("/", str_send, "application/json");
        if (resp != nullptr) {
            rapidjson::Document doc;
            doc.Parse(resp->body.c_str());
            if (!doc.IsObject()) break;

            if (!doc.HasMember("result")) break;
            const rapidjson::Value& v_result = doc["result"];
            if (!v_result.IsObject()) break;

            if (!v_result.HasMember("machineStatus")) break;
            const rapidjson::Value& v_machineStatus = v_result["machineStatus"];
            if (!v_machineStatus.IsString()) break;

            machine_status = v_machineStatus.GetString();
            break;
        }
    }

    if (machine_status == "addingCustomizeInfo" || machine_status == "distributingOrder" ||
        machine_status == "committeeVerifying" || machine_status == "committeeRefused") {
        return MACHINE_STATUS::MS_VERIFY;
    }
    else if (machine_status == "waitingFulfill" || machine_status == "online") {
        return MACHINE_STATUS::MS_ONLINE;
    }
    else if (machine_status == "creating" || machine_status == "rented") {
        return MACHINE_STATUS::MS_RENNTED;
    }
    else {
        return MACHINE_STATUS::MS_ONLINE;
    }
}

bool HttpDBCChainClient::in_verify_time(const std::string& node_id, const std::string& wallet) {
    int64_t cur_block = request_cur_block();

    bool in_time = false;

    for (auto& it : m_addrs) {
        httplib::SSLClient cli(it.second.get_ip(), it.second.get_port());
        cli.set_timeout_sec(5);
        cli.set_read_timeout(10, 0);

        std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"onlineCommittee_getCommitteeOps", "params": [")"
            + wallet + R"(",")" + node_id + R"("]})";
        std::shared_ptr<httplib::Response> resp = cli.Post("/", str_send, "application/json");
        if (resp != nullptr) {
            rapidjson::Document doc;
            doc.Parse(resp->body.c_str());
            if (!doc.IsObject()) break;

            if (!doc.HasMember("result")) break;
            const rapidjson::Value& v_result = doc["result"];
            if (!v_result.IsObject()) break;

            if (!v_result.HasMember("verifyTime")) break;
            const rapidjson::Value& v_verifyTime = v_result["verifyTime"];
            if (!v_verifyTime.IsArray()) break;
            if (v_verifyTime.Size() != 3) break;

            // 是否在验证时间内
            int64_t v1 = v_verifyTime[0].GetInt64();
            int64_t v2 = v_verifyTime[1].GetInt64();
            int64_t v3 = v_verifyTime[2].GetInt64();

            in_time = (cur_block > v1 && cur_block <= v1 + 480) ||
                (cur_block > v2 && cur_block <= v2 + 480) ||
                (cur_block > v3 && cur_block <= v3 + 480);

            break;
        }
    }

    return in_time;
}

int64_t HttpDBCChainClient::request_rent_end(const std::string& node_id, const std::string &wallet) {
    int64_t rent_end = 0;
    
    for (auto& it : m_addrs) {
        httplib::SSLClient cli(it.second.get_ip(), it.second.get_port());
        cli.set_timeout_sec(5);
        cli.set_read_timeout(10, 0);

        std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"rentMachine_getRentOrder", "params": [")"
            + node_id + R"("]})";
        std::shared_ptr<httplib::Response> resp = cli.Post("/", str_send, "application/json");
        if (resp != nullptr) {
            rapidjson::Document doc;
            doc.Parse(resp->body.c_str());
            if (!doc.IsObject()) break;

            if (!doc.HasMember("result")) break;
            const rapidjson::Value& v_result = doc["result"];
            if (!v_result.IsObject()) break;

            if (!v_result.HasMember("renter")) break;
            const rapidjson::Value& v_renter = v_result["renter"];
            if (!v_renter.IsString()) break;
            std::string str_renter = v_renter.GetString();
            if (str_renter != wallet) break;
                
            if (!v_result.HasMember("rentEnd")) break;
            const rapidjson::Value& v_rentEnd = v_result["rentEnd"];
            if (!v_rentEnd.IsNumber()) break;

            int64_t ret = v_rentEnd.GetInt64();
            rent_end = ret;
            break;
        }
    }

    return rent_end;
}
