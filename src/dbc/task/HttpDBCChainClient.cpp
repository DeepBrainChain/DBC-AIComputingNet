#include "HttpDBCChainClient.h"

#include "config/conf_manager.h"
#include "db/db_types/db_rent_order_types.h"
#include "log/log.h"
#include "util/system_info.h"

bool chainScore::operator<(const chainScore& other) const {
    if (block == other.block) return time < other.time;
    return block > other.block;
}

HttpDBCChainClient::HttpDBCChainClient() {}

HttpDBCChainClient::~HttpDBCChainClient() {}

void HttpDBCChainClient::init(const std::vector<std::string>& dbc_chain_addrs) {
    update_chain_order(dbc_chain_addrs);
}

void HttpDBCChainClient::update_chain_order(
    const std::vector<std::string>& dbc_chain_addrs) {
    std::map<chainScore, network::net_address> addrs;
    for (int i = 0; i < dbc_chain_addrs.size(); i++) {
        std::vector<std::string> addr = util::split(dbc_chain_addrs[i], ":");
        if (addr.empty()) continue;

        network::net_address net_addr;
        if (addr.size() > 0) {
            net_addr.set_ip(addr[0]);
        }
        if (addr.size() > 1) {
            net_addr.set_port((uint16_t)atoi(addr[1].c_str()));
        } else {
            net_addr.set_port(443);
        }

        chainScore cs = getChainScore(net_addr, i);
        addrs[cs] = net_addr;
    }
    RwMutex::WriteLock wlock(m_mtx);
    m_addrs.swap(addrs);
}

int64_t HttpDBCChainClient::request_cur_block() {
    int64_t cur_block = 0;

    RwMutex::ReadLock rlock(m_mtx);
    for (auto& it : m_addrs) {
        httplib::SSLClient cli(it.second.get_ip(), it.second.get_port());
        cli.set_timeout_sec(5);
        cli.set_read_timeout(10, 0);

        std::string str_send =
            R"({"jsonrpc": "2.0", "id": 1, "method":"chain_getBlock", "params": []})";
        std::shared_ptr<httplib::Response> resp =
            cli.Post("/", str_send, "application/json");
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
            } catch (...) {
                cur_block = 0;
            }

            break;
        }
    }

    return cur_block;
}

MACHINE_STATUS HttpDBCChainClient::request_machine_status(
    const std::string& node_id) {
    std::string machine_status;

    RwMutex::ReadLock rlock(m_mtx);
    for (auto& it : m_addrs) {
        httplib::SSLClient cli(it.second.get_ip(), it.second.get_port());
        cli.set_timeout_sec(5);
        cli.set_read_timeout(10, 0);

        std::string rpc_api = ConfManager::instance().IsTerminatingRental()
                                  ? "terminatingRental_getMachineInfo"
                                  : "onlineProfile_getMachineInfo";
        std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":")" +
                               rpc_api + R"(", "params": [")" + node_id +
                               R"("]})";
        std::shared_ptr<httplib::Response> resp =
            cli.Post("/", str_send, "application/json");
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

    if (machine_status == "addingCustomizeInfo" ||
        machine_status == "distributingOrder" ||
        machine_status == "committeeVerifying" ||
        machine_status == "committeeRefused") {
        return MACHINE_STATUS::Verify;
    } else if (machine_status == "waitingFulfill" ||
               machine_status == "online") {
        return MACHINE_STATUS::Online;
    } else if (machine_status == "creating" || machine_status == "rented") {
        return MACHINE_STATUS::Rented;
    } else {
        return MACHINE_STATUS::Unknown;
    }
}

bool HttpDBCChainClient::in_verify_time(const std::string& node_id,
                                        const std::string& wallet) {
    int64_t cur_block = request_cur_block();

    bool in_time = false;

    RwMutex::ReadLock rlock(m_mtx);
    for (auto& it : m_addrs) {
        httplib::SSLClient cli(it.second.get_ip(), it.second.get_port());
        cli.set_timeout_sec(5);
        cli.set_read_timeout(10, 0);

        std::string rpc_api = ConfManager::instance().IsTerminatingRental()
                                  ? "terminatingRental_getCommitteeOps"
                                  : "onlineCommittee_getCommitteeOps";
        std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":")" +
                               rpc_api + R"(", "params": [")" + wallet +
                               R"(",")" + node_id + R"("]})";
        std::shared_ptr<httplib::Response> resp =
            cli.Post("/", str_send, "application/json");
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

            // 是否在验证时间内，4 个小时
            int64_t v1 = v_verifyTime[0].GetInt64();
            int64_t v2 = v_verifyTime[1].GetInt64();
            int64_t v3 = v_verifyTime[2].GetInt64();

            in_time = (cur_block > v1 && cur_block <= v1 + 2400) ||
                      (cur_block > v2 && cur_block <= v2 + 2400) ||
                      (cur_block > v3 && cur_block <= v3 + 2400);

            break;
        }
    }

    return in_time;
}

// int64_t HttpDBCChainClient::request_rent_end(const std::string& node_id,
// const std::string &wallet) {
//     int64_t rent_end = 0;

//     RwMutex::ReadLock rlock(m_mtx);
//     for (auto& it : m_addrs) {
//         httplib::SSLClient cli(it.second.get_ip(), it.second.get_port());
//         cli.set_timeout_sec(5);
//         cli.set_read_timeout(10, 0);

//         std::string str_send = R"({"jsonrpc": "2.0", "id": 1,
//         "method":"rentMachine_getRentOrder", "params": [")"
//             + node_id + R"("]})";
//         std::shared_ptr<httplib::Response> resp = cli.Post("/", str_send,
//         "application/json"); if (resp != nullptr) {
//             rapidjson::Document doc;
//             doc.Parse(resp->body.c_str());
//             if (!doc.IsObject()) break;

//             if (!doc.HasMember("result")) break;
//             const rapidjson::Value& v_result = doc["result"];
//             if (!v_result.IsObject()) break;

//             if (!v_result.HasMember("renter")) break;
//             const rapidjson::Value& v_renter = v_result["renter"];
//             if (!v_renter.IsString()) break;
//             std::string str_renter = v_renter.GetString();
//             if (str_renter != wallet) break;

//             if (!v_result.HasMember("rentEnd")) break;
//             const rapidjson::Value& v_rentEnd = v_result["rentEnd"];
//             if (!v_rentEnd.IsNumber()) break;

//             int64_t ret = v_rentEnd.GetInt64();
//             rent_end = ret;
//             break;
//         }
//     }

//     return rent_end;
// }

void HttpDBCChainClient::request_cur_renter(const std::string& node_id,
                                            std::string& renter,
                                            int64_t& rent_end) {
    RwMutex::ReadLock rlock(m_mtx);
    for (auto& it : m_addrs) {
        httplib::SSLClient cli(it.second.get_ip(), it.second.get_port());
        cli.set_timeout_sec(5);
        cli.set_read_timeout(10, 0);

        std::string rpc_api = ConfManager::instance().IsTerminatingRental()
                                  ? "terminatingRental_getRentOrder"
                                  : "rentMachine_getRentOrder";
        std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":")" +
                               rpc_api + R"(", "params": [")" + node_id +
                               R"("]})";
        std::shared_ptr<httplib::Response> resp =
            cli.Post("/", str_send, "application/json");
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

bool HttpDBCChainClient::getCommitteeUploadInfo(const std::string& node_id,
                                                CommitteeUploadInfo& info) {
    bool bret = false;

    RwMutex::ReadLock rlock(m_mtx);
    for (auto& it : m_addrs) {
        httplib::SSLClient cli(it.second.get_ip(), it.second.get_port());
        cli.set_timeout_sec(5);
        cli.set_read_timeout(10, 0);

        std::string rpc_api = ConfManager::instance().IsTerminatingRental()
                                  ? "terminatingRental_getMachineInfo"
                                  : "onlineProfile_getMachineInfo";
        std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":")" +
                               rpc_api + R"(", "params": [")" + node_id +
                               R"("]})";
        std::shared_ptr<httplib::Response> resp =
            cli.Post("/", str_send, "application/json");
        if (resp != nullptr) {
            rapidjson::Document doc;
            doc.Parse(resp->body.c_str());
            if (!doc.IsObject()) break;

            if (!doc.HasMember("result")) break;
            const rapidjson::Value& v_result = doc["result"];
            if (!v_result.IsObject()) break;

            if (!v_result.HasMember("machineInfoDetail")) break;
            const rapidjson::Value& v_machineInfoDetail =
                v_result["machineInfoDetail"];
            if (!v_machineInfoDetail.IsObject()) break;

            if (!v_machineInfoDetail.HasMember("committee_upload_info")) break;
            const rapidjson::Value& v_committeeUploadInfo =
                v_machineInfoDetail["committee_upload_info"];
            if (!v_committeeUploadInfo.IsObject()) break;

            if (v_committeeUploadInfo.HasMember("machine_id") &&
                v_committeeUploadInfo["machine_id"].IsArray()) {
                const rapidjson::Value& v_array =
                    v_committeeUploadInfo["machine_id"];
                std::vector<unsigned char> bytes;
                for (const auto& v : v_array.GetArray()) {
                    bytes.push_back(v.GetUint());
                }
                info.machine_id = std::string((char*)&bytes[0], bytes.size());
            }

            if (v_committeeUploadInfo.HasMember("gpu_type") &&
                v_committeeUploadInfo["gpu_type"].IsArray()) {
                const rapidjson::Value& v_array =
                    v_committeeUploadInfo["gpu_type"];
                std::vector<unsigned char> bytes;
                for (const auto& v : v_array.GetArray()) {
                    bytes.push_back(v.GetUint());
                }
                info.gpu_type = std::string((char*)&bytes[0], bytes.size());
            }

            if (v_committeeUploadInfo.HasMember("gpu_num") &&
                v_committeeUploadInfo["gpu_num"].IsUint()) {
                info.gpu_num = v_committeeUploadInfo["gpu_num"].GetUint();
            } else {
                info.gpu_num = 0;
            }

            if (v_committeeUploadInfo.HasMember("cuda_core") &&
                v_committeeUploadInfo["cuda_core"].IsUint()) {
                info.cuda_core = v_committeeUploadInfo["cuda_core"].GetUint();
            } else {
                info.cuda_core = 0;
            }

            if (v_committeeUploadInfo.HasMember("gpu_mem") &&
                v_committeeUploadInfo["gpu_mem"].IsUint64()) {
                info.gpu_mem = v_committeeUploadInfo["gpu_mem"].GetUint64();
            } else {
                info.gpu_mem = 0;
            }

            if (v_committeeUploadInfo.HasMember("calc_point") &&
                v_committeeUploadInfo["calc_point"].IsUint64()) {
                info.calc_point =
                    v_committeeUploadInfo["calc_point"].GetUint64();
            } else {
                info.calc_point = 0;
            }

            if (v_committeeUploadInfo.HasMember("sys_disk") &&
                v_committeeUploadInfo["sys_disk"].IsUint64()) {
                info.sys_disk = v_committeeUploadInfo["sys_disk"].GetUint64();
            } else {
                info.sys_disk = 0;
            }

            if (v_committeeUploadInfo.HasMember("data_disk") &&
                v_committeeUploadInfo["data_disk"].IsUint64()) {
                info.data_disk = v_committeeUploadInfo["data_disk"].GetUint64();
            } else {
                info.data_disk = 0;
            }

            if (v_committeeUploadInfo.HasMember("cpu_type") &&
                v_committeeUploadInfo["cpu_type"].IsArray()) {
                const rapidjson::Value& v_array =
                    v_committeeUploadInfo["cpu_type"];
                std::vector<unsigned char> bytes;
                for (const auto& v : v_array.GetArray()) {
                    bytes.push_back(v.GetUint());
                }
                info.cpu_type = std::string((char*)&bytes[0], bytes.size());
            }

            if (v_committeeUploadInfo.HasMember("cpu_core_num") &&
                v_committeeUploadInfo["cpu_core_num"].IsUint()) {
                info.cpu_core_num =
                    v_committeeUploadInfo["cpu_core_num"].GetUint();
            } else {
                info.cpu_core_num = 0;
            }

            if (v_committeeUploadInfo.HasMember("cpu_rate") &&
                v_committeeUploadInfo["cpu_rate"].IsUint64()) {
                info.cpu_rate = v_committeeUploadInfo["cpu_rate"].GetUint64();
            } else {
                info.cpu_rate = 0;
            }

            if (v_committeeUploadInfo.HasMember("mem_num") &&
                v_committeeUploadInfo["mem_num"].IsUint64()) {
                info.mem_num = v_committeeUploadInfo["mem_num"].GetUint64();
            } else {
                info.mem_num = 0;
            }

            if (v_committeeUploadInfo.HasMember("is_support") &&
                v_committeeUploadInfo["is_support"].IsBool()) {
                info.is_support = v_committeeUploadInfo["is_support"].GetBool();
            } else {
                info.is_support = 0;
            }
            bret = true;

            break;
        }
    }

    return bret;
}

chainScore HttpDBCChainClient::getChainScore(const network::net_address& addr,
                                             int delta) const {
    int64_t cur_block = 0;
    int32_t time = 0xFFFF + delta;

    do {
        struct timeval tv1 {};
        gettimeofday(&tv1, 0);

        httplib::SSLClient cli(addr.get_ip(), addr.get_port());
        cli.set_timeout_sec(5);
        cli.set_read_timeout(10, 0);

        std::string str_send =
            R"({"jsonrpc": "2.0", "id": 1, "method":"chain_getBlock", "params": []})";
        std::shared_ptr<httplib::Response> resp =
            cli.Post("/", str_send, "application/json");
        if (!resp) break;

        struct timeval tv2 {};
        gettimeofday(&tv2, 0);
        time = (tv2.tv_sec * 1000 + tv2.tv_usec / 1000) -
               (tv1.tv_sec * 1000 + tv1.tv_usec / 1000);

        rapidjson::Document doc;
        rapidjson::ParseResult ok = doc.Parse(resp->body.c_str());
        if (!ok) break;

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
        } catch (...) {
            cur_block = 0;
        }
    } while (0);

    return {cur_block, time};
}

std::shared_ptr<dbc::db_rent_order> HttpDBCChainClient::getRentOrder(
    const std::string& node_id, const std::string& rent_order) {
    bool is_order = !rent_order.empty();
    RwMutex::ReadLock rlock(m_mtx);
    for (auto& it : m_addrs) {
        httplib::SSLClient cli(it.second.get_ip(), it.second.get_port());
        cli.set_timeout_sec(5);
        cli.set_read_timeout(10, 0);

        std::string params = rent_order;
        if (!is_order) params = "\"" + node_id + "\"";

        std::string rpc_api = ConfManager::instance().IsTerminatingRental()
                                  ? "terminatingRental_getRentOrder"
                                  : "rentMachine_getRentOrder";
        std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":")" +
                               rpc_api + R"(", "params": [)" + params + R"(]})";
        std::shared_ptr<httplib::Response> resp =
            cli.Post("/", str_send, "application/json");
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
            std::string renter = v_renter.GetString();

            if (!v_result.HasMember("rentEnd")) break;
            const rapidjson::Value& v_rentEnd = v_result["rentEnd"];
            if (!v_rentEnd.IsNumber()) break;
            uint64_t rent_end = v_rentEnd.GetUint64();

            if (!v_result.HasMember("rentStatus")) break;
            const rapidjson::Value& v_rentStatus = v_result["rentStatus"];
            if (!v_rentStatus.IsString()) break;
            std::string rent_status = v_rentStatus.GetString();

            std::shared_ptr<dbc::db_rent_order> pro =
                std::make_shared<dbc::db_rent_order>();
            pro->__set_id(is_order ? rent_order : renter);
            pro->__set_renter(renter);
            pro->__set_rent_end(rent_end);
            pro->__set_rent_status(rent_status);

            std::vector<int32_t> gpus;
            if (is_order) {
                if (!v_result.HasMember("gpuNum")) break;
                const rapidjson::Value& v_gpuNum = v_result["gpuNum"];
                if (!v_gpuNum.IsNumber()) break;
                int32_t gpu_num = v_gpuNum.GetInt();
                // if (gpu_num <= 0) break;
                pro->__set_gpu_num(gpu_num);

                if (!v_result.HasMember("gpuIndex")) break;
                const rapidjson::Value& v_gpuIndex = v_result["gpuIndex"];
                if (!v_gpuIndex.IsArray()) break;
                for (const auto& v_index : v_gpuIndex.GetArray()) {
                    if (v_index.IsInt())
                        gpus.push_back(v_index.GetInt());
                    else
                        break;
                }
                // if (gpus.empty()) break;
                pro->__set_gpu_index(gpus);
            } else {
                int gpu_count = SystemInfo::instance().GetGpuInfo().size();
                pro->__set_gpu_num(gpu_count);
                for (int i = 0; i < gpu_count; i++) gpus.push_back(i);
                pro->__set_gpu_index(gpus);
            }

            return std::move(pro);
        }
    }
    return nullptr;
}

bool HttpDBCChainClient::getRentOrderList(const std::string& node_id,
                                          std::set<std::string>& orders) {
    bool bret = false;
    RwMutex::ReadLock rlock(m_mtx);
    for (auto& it : m_addrs) {
        orders.clear();

        httplib::SSLClient cli(it.second.get_ip(), it.second.get_port());
        cli.set_timeout_sec(5);
        cli.set_read_timeout(10, 0);

        std::string rpc_api = ConfManager::instance().IsTerminatingRental()
                                  ? "terminatingRental_getMachineRentId"
                                  : "rentMachine_getMachineRentId";
        std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":")" +
                               rpc_api + R"(", "params": [")" + node_id +
                               R"("]})";
        std::shared_ptr<httplib::Response> resp =
            cli.Post("/", str_send, "application/json");
        if (resp != nullptr) {
            rapidjson::Document doc;
            doc.Parse(resp->body.c_str());
            if (!doc.IsObject()) break;

            if (!doc.HasMember("result")) break;
            const rapidjson::Value& v_result = doc["result"];
            if (!v_result.IsObject()) break;

            if (!v_result.HasMember("rentOrder")) break;
            const rapidjson::Value& v_rentOrder = v_result["rentOrder"];
            if (!v_rentOrder.IsArray()) break;

            for (const auto& v_order : v_rentOrder.GetArray()) {
                if (v_order.IsUint64())
                    orders.insert(std::to_string(v_order.GetUint64()));
                else
                    break;
            }
            bret = true;
            break;
        }
    }
    return bret;
}

bool HttpDBCChainClient::isMachineRenter(const std::string& node_id,
                                         const std::string& wallet) {
    bool bret = false;

    RwMutex::ReadLock rlock(m_mtx);
    for (auto& it : m_addrs) {
        httplib::SSLClient cli(it.second.get_ip(), it.second.get_port());
        cli.set_timeout_sec(5);
        cli.set_read_timeout(10, 0);

        std::string rpc_api = ConfManager::instance().IsTerminatingRental()
                                  ? "terminatingRental_isMachineRenter"
                                  : "rentMachine_isMachineRenter";
        std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":")" +
                               rpc_api + R"(", "params": [")" + node_id +
                               R"(",")" + wallet + R"("]})";
        std::shared_ptr<httplib::Response> resp =
            cli.Post("/", str_send, "application/json");
        if (resp != nullptr) {
            rapidjson::Document doc;
            doc.Parse(resp->body.c_str());
            if (!doc.IsObject()) break;

            if (!doc.HasMember("result")) break;
            if (!doc["result"].IsBool()) break;
            bret = doc["result"].GetBool();
            break;
        }
    }

    return bret;
}
