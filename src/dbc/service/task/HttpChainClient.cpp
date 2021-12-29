#include "HttpChainClient.h"
#include "config/conf_manager.h"
#include "log/log.h"

HttpChainClient::HttpChainClient() {

}

HttpChainClient::~HttpChainClient() {
    delete m_httpclient;
}

bool HttpChainClient::connect_chain() {
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_httpclient != nullptr && m_httpclient->is_valid()) {
        return true;
    }

    delete m_httpclient;
    m_httpclient = nullptr;

    std::map<int64_t, std::string> mp;
    std::vector<std::string> vec = conf_manager::instance().get_dbc_chain_domain();
    for (int i = 0; i < vec.size(); i++) {
        std::vector<std::string> addr = util::split(vec[i], ":");
        if (!addr.empty()) {
            httplib::SSLClient cli(addr[0], addr.size() > 1 ? atoi(addr[1].c_str()) : 443);
            if (cli.is_valid()) {
                struct timeval tv1{};
                gettimeofday(&tv1, 0);

                std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"chain_getBlock", "params": []})";
                std::shared_ptr<httplib::Response> resp = cli.Post("/", str_send, "application/json");
                if (resp == nullptr) {
                    mp[0xFFFFFFFFL + i] = vec[i];
                    continue;
                }

                struct timeval tv2{};
                gettimeofday(&tv2, 0);

                int64_t msec = (tv2.tv_sec * 1000 + tv2.tv_usec / 1000) - (tv1.tv_sec * 1000 + tv1.tv_usec / 1000);
                mp[msec] = vec[i];
            }
        }
    }

    for (auto& it : mp) {
        delete m_httpclient;
        m_httpclient = nullptr;

        std::vector<std::string> addr = util::split(it.second, ":");
        if (!addr.empty()) {
            m_httpclient = new httplib::SSLClient(addr[0], addr.size() > 1 ? atoi(addr[1].c_str()) : 443);
            if (m_httpclient->is_valid())
                break;
        }
    }

    return (m_httpclient != nullptr && m_httpclient->is_valid());
}

std::string HttpChainClient::request_machine_status() {
    if (!connect_chain()) return "";

    std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"onlineProfile_getMachineInfo", "params": [")"
                           + conf_manager::instance().get_node_id() + R"("]})";
    std::shared_ptr<httplib::Response> resp = m_httpclient->Post("/", str_send, "application/json");
    if (resp != nullptr) {
        rapidjson::Document doc;
        doc.Parse(resp->body.c_str());
        if (!doc.IsObject()) return "";

        if (!doc.HasMember("result")) return "";
        const rapidjson::Value &v_result = doc["result"];
        if (!v_result.IsObject()) return "";

        if (!v_result.HasMember("machineStatus")) return "";
        const rapidjson::Value &v_machineStatus = v_result["machineStatus"];
        if (!v_machineStatus.IsString()) return "";

        std::string machine_status = v_machineStatus.GetString();
        return machine_status;
    } else {
        return "";
    }
}

int64_t HttpChainClient::request_rent_end(const std::string &wallet) {
    if (!connect_chain()) return 0;

    int64_t cur_rent_end = 0;
    std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"rentMachine_getRentOrder", "params": [")"
                           + conf_manager::instance().get_node_id() + R"("]})";
    std::shared_ptr<httplib::Response> resp = m_httpclient->Post("/", str_send, "application/json");
    if (resp != nullptr) {
        do {
            rapidjson::Document doc;
            doc.Parse(resp->body.c_str());
            if (!doc.IsObject()) break;

            if (!doc.HasMember("result")) break;
            const rapidjson::Value &v_result = doc["result"];
            if (!v_result.IsObject()) break;

            if (!v_result.HasMember("renter")) break;
            const rapidjson::Value &v_renter = v_result["renter"];
            if (!v_renter.IsString()) break;
            std::string str_renter = v_renter.GetString();
            if (str_renter != wallet) {
                break;
            }

            if (!v_result.HasMember("rentEnd")) break;
            const rapidjson::Value &v_rentEnd = v_result["rentEnd"];
            if (!v_rentEnd.IsNumber()) break;

            int64_t rentEnd = v_rentEnd.GetInt64();
            if (rentEnd > 0) {
                cur_rent_end = rentEnd;
            }
        } while (0);
    }

    return cur_rent_end;
}

int64_t HttpChainClient::request_cur_block() {
    if (!connect_chain()) return 0;

    std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"chain_getBlock", "params": []})";
    std::shared_ptr<httplib::Response> resp = m_httpclient->Post("/", str_send, "application/json");
    if (resp != nullptr) {
        rapidjson::Document doc;
        doc.Parse(resp->body.c_str());
        if (!doc.IsObject()) return 0;

        if (!doc.HasMember("result")) return 0;
        const rapidjson::Value &v_result = doc["result"];
        if (!v_result.IsObject()) return 0;

        if (!v_result.HasMember("block")) return 0;
        const rapidjson::Value &v_block = v_result["block"];
        if (!v_block.IsObject()) return 0;

        if (!v_block.HasMember("header")) return 0;
        const rapidjson::Value &v_header = v_block["header"];
        if (!v_header.IsObject()) return 0;

        if (!v_header.HasMember("number")) return 0;
        const rapidjson::Value &v_number = v_header["number"];
        if (!v_number.IsString()) return 0;

        char* p;
        int64_t n = 0;
        try {
            n = strtol(v_number.GetString(), &p, 16);
        } catch(...) {
            n = 0;
        }

        return n;
    } else {
        return 0;
    }
}

bool HttpChainClient::in_verify_time(const std::string &wallet) {
    if (!connect_chain()) return false;

    int64_t cur_block = request_cur_block();
    if (cur_block <= 0) return false;

    std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"onlineCommittee_getCommitteeOps", "params": [")"
                           + wallet + R"(",")" + conf_manager::instance().get_node_id() + R"("]})";
    std::shared_ptr<httplib::Response> resp = m_httpclient->Post("/", str_send, "application/json");
    if (resp != nullptr) {
        rapidjson::Document doc;
        doc.Parse(resp->body.c_str());
        if (!doc.IsObject()) {
            LOG_ERROR << "parse response failed";
            return false;
        }

        if (!doc.HasMember("result")) {
            LOG_ERROR << "parse response failed";
            return false;
        }

        const rapidjson::Value &v_result = doc["result"];
        if (!v_result.IsObject()) {
            LOG_ERROR << "parse response failed";
            return false;
        }

        if (!v_result.HasMember("verifyTime")) {
            LOG_ERROR << "parse response failed";
            return false;
        }

        const rapidjson::Value &v_verifyTime = v_result["verifyTime"];
        if (!v_verifyTime.IsArray()) {
            LOG_ERROR << "parse response failed";
            return false;
        }

        if (v_verifyTime.Size() != 3) {
            LOG_ERROR << "verify_time's size != 3";
            return false;
        }

        // 是否在验证时间内
        int64_t v1 = v_verifyTime[0].GetInt64();
        int64_t v2 = v_verifyTime[1].GetInt64();
        int64_t v3 = v_verifyTime[2].GetInt64();

        if ((cur_block > v1 && cur_block <= v1 + 480) ||
            (cur_block > v2 && cur_block <= v2 + 480) ||
            (cur_block > v3 && cur_block <= v3 + 480))
        {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}
