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
        return MACHINE_STATUS::Verify;
    }
    else if (machine_status == "waitingFulfill" || machine_status == "online") {
        return MACHINE_STATUS::Online;
    }
    else if (machine_status == "creating" || machine_status == "rented") {
        return MACHINE_STATUS::Rented;
    }
    else {
        return MACHINE_STATUS::Online;
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

void HttpDBCChainClient::request_cur_renter(const std::string& node_id, std::string& renter, int64_t& rent_end) {
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
            renter = v_renter.GetString();

            if (!v_result.HasMember("rentEnd")) break;
            const rapidjson::Value& v_rentEnd = v_result["rentEnd"];
            if (!v_rentEnd.IsNumber()) break; 
            rent_end = v_rentEnd.GetInt64();
            
            break;
        }
    }
}

bool HttpDBCChainClient::getCommitteeUploadInfo(const std::string& node_id, CommitteeUploadInfo& info) {
    bool bret = false;

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

            if (!v_result.HasMember("machineInfoDetail")) break;
            const rapidjson::Value& v_machineInfoDetail = v_result["machineInfoDetail"];
            if (!v_machineInfoDetail.IsObject()) break;

            if (!v_machineInfoDetail.HasMember("committee_upload_info")) break;
            const rapidjson::Value& v_committeeUploadInfo = v_machineInfoDetail["committee_upload_info"];
            if (!v_committeeUploadInfo.IsObject()) break;

            if (v_committeeUploadInfo.HasMember("machine_id") && v_committeeUploadInfo["machine_id"].IsArray()) {
                const rapidjson::Value& v_array = v_committeeUploadInfo["machine_id"];
                std::vector<unsigned char> bytes;
                for (const auto& v : v_array.GetArray()) {
                    bytes.push_back(v.GetUint());
                }
                info.machine_id = std::string((char*)&bytes[0], bytes.size());
            }

            if (v_committeeUploadInfo.HasMember("gpu_type") && v_committeeUploadInfo["gpu_type"].IsArray()) {
                const rapidjson::Value& v_array = v_committeeUploadInfo["gpu_type"];
                std::vector<unsigned char> bytes;
                for (const auto& v : v_array.GetArray()) {
                    bytes.push_back(v.GetUint());
                }
                info.gpu_type = std::string((char*)&bytes[0], bytes.size());
            }

            if (v_committeeUploadInfo.HasMember("gpu_num") && v_committeeUploadInfo["gpu_num"].IsUint()) {
                info.gpu_num = v_committeeUploadInfo["gpu_num"].GetUint();
            } else {
                info.gpu_num = 0;
            }

            if (v_committeeUploadInfo.HasMember("cuda_core") && v_committeeUploadInfo["cuda_core"].IsUint()) {
                info.cuda_core = v_committeeUploadInfo["cuda_core"].GetUint();
            } else {
                info.cuda_core = 0;
            }

            if (v_committeeUploadInfo.HasMember("gpu_mem") && v_committeeUploadInfo["gpu_mem"].IsUint64()) {
                info.gpu_mem = v_committeeUploadInfo["gpu_mem"].GetUint64();
            } else {
                info.gpu_mem = 0;
            }

            if (v_committeeUploadInfo.HasMember("calc_point") && v_committeeUploadInfo["calc_point"].IsUint64()) {
                info.calc_point = v_committeeUploadInfo["calc_point"].GetUint64();
            } else {
                info.calc_point = 0;
            }

            if (v_committeeUploadInfo.HasMember("sys_disk") && v_committeeUploadInfo["sys_disk"].IsUint64()) {
                info.sys_disk = v_committeeUploadInfo["sys_disk"].GetUint64();
            } else {
                info.sys_disk = 0;
            }

            if (v_committeeUploadInfo.HasMember("data_disk") && v_committeeUploadInfo["data_disk"].IsUint64()) {
                info.data_disk = v_committeeUploadInfo["data_disk"].GetUint64();
            } else {
                info.data_disk = 0;
            }

            if (v_committeeUploadInfo.HasMember("cpu_type") && v_committeeUploadInfo["cpu_type"].IsArray()) {
                const rapidjson::Value& v_array = v_committeeUploadInfo["cpu_type"];
                std::vector<unsigned char> bytes;
                for (const auto& v : v_array.GetArray()) {
                    bytes.push_back(v.GetUint());
                }
                info.cpu_type = std::string((char*)&bytes[0], bytes.size());
            }

            if (v_committeeUploadInfo.HasMember("cpu_core_num") && v_committeeUploadInfo["cpu_core_num"].IsUint()) {
                info.cpu_core_num = v_committeeUploadInfo["cpu_core_num"].GetUint();
            } else {
                info.cpu_core_num = 0;
            }

            if (v_committeeUploadInfo.HasMember("cpu_rate") && v_committeeUploadInfo["cpu_rate"].IsUint64()) {
                info.cpu_rate = v_committeeUploadInfo["cpu_rate"].GetUint64();
            } else {
                info.cpu_rate = 0;
            }

            if (v_committeeUploadInfo.HasMember("mem_num") && v_committeeUploadInfo["mem_num"].IsUint64()) {
                info.mem_num = v_committeeUploadInfo["mem_num"].GetUint64();
            } else {
                info.mem_num = 0;
            }

            if (v_committeeUploadInfo.HasMember("is_support") && v_committeeUploadInfo["is_support"].IsBool()) {
                info.is_support = v_committeeUploadInfo["is_support"].GetBool();
            } else {
                info.is_support = 0;
            }
            bret = true;
        }
    }

    return bret;
}
