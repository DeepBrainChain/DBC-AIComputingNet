#include "HttpChainClient.h"
#include "config/conf_manager.h"
#include "log/log.h"

HttpChainClient::HttpChainClient() {
    if (m_httpclient == nullptr)
        m_httpclient = new httplib::SSLClient(conf_manager::instance().get_dbc_chain_domain(), 443);
}

HttpChainClient::~HttpChainClient() {
    delete m_httpclient;
}

std::string HttpChainClient::request_machine_status() {
    if (!m_httpclient->is_valid()) return "";

    std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"onlineProfile_getMachineInfo", "params": [")"
                           + conf_manager::instance().get_node_id() + R"("]})";
    std::shared_ptr<httplib::Response> resp = m_httpclient->Post("/", str_send, "application/json");
    if (resp != nullptr) {
        rapidjson::Document doc;
        doc.Parse(resp->body.c_str());
        if (!doc.IsObject()) {
            LOG_ERROR << "parse response failed";
            return "";
        }

        if (!doc.HasMember("result")) {
            LOG_ERROR << "parse response failed";
            return "";
        }

        const rapidjson::Value &v_result = doc["result"];
        if (!v_result.IsObject()) {
            LOG_ERROR << "parse response failed";
            return "";
        }

        if (!v_result.HasMember("machineStatus")) {
            LOG_ERROR << "parse response failed";
            return "";
        }

        const rapidjson::Value &v_machineStatus = v_result["machineStatus"];
        if (!v_machineStatus.IsString()) {
            LOG_ERROR << "parse response failed";
            return "";
        }

        std::string machine_status = v_machineStatus.GetString();
        return machine_status;
    } else {
        return "";
    }
}

int64_t HttpChainClient::request_rent_end(const std::string &wallet) {
    int64_t cur_rent_end = 0;
    if (!m_httpclient->is_valid()) return cur_rent_end;

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

            if (v_result.HasMember("renter")) {
                const rapidjson::Value &v_renter = v_result["renter"];
                if (!v_renter.IsString()) break;

                std::string str_renter = v_renter.GetString();
                if (str_renter != wallet) {
                    break;
                }
            }

            if (v_result.HasMember("rentEnd")) {
                const rapidjson::Value &v_rentEnd = v_result["rentEnd"];
                if (!v_rentEnd.IsNumber()) break;

                int64_t rentEnd = v_rentEnd.GetInt64();
                if (rentEnd > 0) {
                    cur_rent_end = rentEnd;
                    break;
                }
            }
        } while (0);
    }

    return cur_rent_end;
}

int64_t HttpChainClient::request_cur_block() {
    if (!m_httpclient->is_valid()) return 0;

    std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"chain_getBlock", "params": []}")";
    std::shared_ptr<httplib::Response> resp = m_httpclient->Post("/", str_send, "application/json");
    if (resp != nullptr) {
        rapidjson::Document doc;
        doc.Parse(resp->body.c_str());
        if (!doc.IsObject()) {
            return 0;
        }

        if (!doc.HasMember("result")) {
            return 0;
        }

        const rapidjson::Value &v_result = doc["result"];
        if (!v_result.IsObject()) {
            return 0;
        }

        if (!v_result.HasMember("block")) {
            return 0;
        }

        const rapidjson::Value &v_block = v_result["block"];
        if (!v_block.IsObject()) {
            return 0;
        }

        if (!v_block.HasMember("header")) {
            return 0;
        }

        const rapidjson::Value &v_header = v_block["header"];
        if (!v_header.IsObject()) {
            return 0;
        }

        if (!v_header.HasMember("number")) {
            return 0;
        }

        const rapidjson::Value &v_number = v_header["number"];
        if (!v_number.IsString()) {
            return 0;
        }

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
