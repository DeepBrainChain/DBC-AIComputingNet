#include "node_request_service.h"
#include <boost/property_tree/json_parser.hpp>
#include <boost/exception/all.hpp>
#include "server/server.h"
#include "config/conf_manager.h"
#include "service_module/service_message_id.h"
#include "../message/matrix_types.h"
#include "../message/protocol_coder/matrix_coder.h"
#include <boost/thread/thread.hpp>
#include "../message/service_topic.h"
#include <boost/format.hpp>
#include <boost/algorithm/string/join.hpp>
#include "../message/message_id.h"
#include "service_module/service_name.h"
#include "util/base64.h"
#include "tweetnacl/tools.h"
#include "tweetnacl/randombytes.h"
#include "tweetnacl/tweetnacl.h"

std::string get_gpu_spec(const std::string& s) {
    if (s.empty()) {
        return "";
    }

    std::string rt;
    std::stringstream ss;
    ss << s;
    boost::property_tree::ptree pt;
    try {
        boost::property_tree::read_json(ss, pt);
        rt = pt.get<std::string>("env.NVIDIA_VISIBLE_DEVICES");
        if (!rt.empty()) {
            util::trim(rt);
        }
    }
    catch (...) {

    }

    return rt;
}

std::string get_is_update(const std::string& s) {
    if (s.empty()) return "";

    std::string operation;
    std::stringstream ss;
    ss << s;
    boost::property_tree::ptree pt;

    try {
        boost::property_tree::read_json(ss, pt);
        if (pt.count("operation") != 0) {
            operation = pt.get<std::string>("operation");
        }
    }
    catch (...) {

    }

    return operation;
}

node_request_service::~node_request_service() {
    if (m_is_computing_node) {
        remove_timer(m_training_task_timer_id);
        remove_timer(m_prune_task_timer_id);
    }
}

int32_t node_request_service::init(bpo::variables_map &options) {
    if (options.count(SERVICE_NAME_AI_TRAINING)) {
        m_is_computing_node = true;
        add_self_to_servicelist(options);
    }

    service_module::init(options);

    if (m_is_computing_node) {
        auto fresult = m_task_scheduler.Init();
        int32_t ret = std::get<0>(fresult);
        if (ret != E_SUCCESS) {
            return E_DEFAULT;
        } else {
            return E_SUCCESS;
        }
    }

    return E_SUCCESS;
}

void node_request_service::add_self_to_servicelist(bpo::variables_map &options) {
    dbc::node_service_info info;
    info.service_list.emplace_back(SERVICE_NAME_AI_TRAINING);

    if (options.count("name")) {
        auto name_str = options["name"].as<std::string>();
        info.__set_name(name_str);
    } else {
        info.__set_name("null");
    }

    auto tnow = std::time(nullptr);
    info.__set_time_stamp(tnow);

    std::map<std::string, std::string> kvs;
    kvs["version"] = SystemInfo::instance().get_version();

    /*
    int32_t count = m_task_scheduler.GetRunningTaskSize();
    std::string state;
    if (count <= 0) {
        state = "idle";
    }
    else {
        state = "busy(" + std::to_string(count) + ")";
    }
    kvs["state"] = state;
    */

    kvs["pub_key"] = conf_manager::instance().get_pub_key();
    info.__set_kvs(kvs);

    service_info_collection::instance().add(conf_manager::instance().get_node_id(), info);
}

void node_request_service::init_timer() {
    if (m_is_computing_node) {
        // 10s
        m_timer_invokers[AI_TRAINING_TASK_TIMER] = std::bind(&node_request_service::on_training_task_timer, this, std::placeholders::_1);
        m_training_task_timer_id = this->add_timer(AI_TRAINING_TASK_TIMER, 10 * 1000, ULLONG_MAX, "");

        // 1min
        m_timer_invokers[AI_PRUNE_TASK_TIMER] = std::bind(&node_request_service::on_prune_task_timer, this, std::placeholders::_1);
        m_prune_task_timer_id = this->add_timer(AI_PRUNE_TASK_TIMER, 60 * 1000, ULLONG_MAX, "");
    }

    // 10s
    m_timer_invokers[SERVICE_BROADCAST_TIMER] = std::bind(&node_request_service::on_timer_service_broadcast, this, std::placeholders::_1);
    add_timer(SERVICE_BROADCAST_TIMER, 10 * 1000, ULLONG_MAX, "");
}

void node_request_service::init_invoker() {
    invoker_type invoker;
    BIND_MESSAGE_INVOKER(NODE_CREATE_TASK_REQ, &node_request_service::on_node_create_task_req)
    BIND_MESSAGE_INVOKER(NODE_START_TASK_REQ, &node_request_service::on_node_start_task_req)
    BIND_MESSAGE_INVOKER(NODE_RESTART_TASK_REQ, &node_request_service::on_node_restart_task_req)
    BIND_MESSAGE_INVOKER(NODE_STOP_TASK_REQ, &node_request_service::on_node_stop_task_req)
    BIND_MESSAGE_INVOKER(NODE_RESET_TASK_REQ, &node_request_service::on_node_reset_task_req)
    BIND_MESSAGE_INVOKER(NODE_DELETE_TASK_REQ, &node_request_service::on_node_delete_task_req)
    BIND_MESSAGE_INVOKER(NODE_TASK_LOGS_REQ, &node_request_service::on_node_task_logs_req)
    BIND_MESSAGE_INVOKER(NODE_LIST_TASK_REQ, &node_request_service::on_node_list_task_req)
    BIND_MESSAGE_INVOKER(NODE_QUERY_NODE_INFO_REQ, &node_request_service::on_node_query_node_info_req)
    BIND_MESSAGE_INVOKER(SERVICE_BROADCAST_REQ, &node_request_service::on_net_service_broadcast_req)
    BIND_MESSAGE_INVOKER(NODE_SESSION_ID_REQ, &node_request_service::on_node_session_id_req)
}

void node_request_service::init_subscription() {
    SUBSCRIBE_BUS_MESSAGE(NODE_CREATE_TASK_REQ);
    SUBSCRIBE_BUS_MESSAGE(NODE_START_TASK_REQ);
    SUBSCRIBE_BUS_MESSAGE(NODE_RESTART_TASK_REQ);
    SUBSCRIBE_BUS_MESSAGE(NODE_STOP_TASK_REQ);
    SUBSCRIBE_BUS_MESSAGE(NODE_RESET_TASK_REQ);
    SUBSCRIBE_BUS_MESSAGE(NODE_DELETE_TASK_REQ);
    SUBSCRIBE_BUS_MESSAGE(NODE_TASK_LOGS_REQ);
    SUBSCRIBE_BUS_MESSAGE(NODE_LIST_TASK_REQ);
    SUBSCRIBE_BUS_MESSAGE(NODE_QUERY_NODE_INFO_REQ)
    SUBSCRIBE_BUS_MESSAGE(SERVICE_BROADCAST_REQ);
    SUBSCRIBE_BUS_MESSAGE(NODE_SESSION_ID_REQ);
}

bool node_request_service::hit_node(const std::vector<std::string>& peer_node_list, const std::string& node_id) {
    bool hit = false;
    auto it = peer_node_list.begin();
    for (; it != peer_node_list.end(); it++) {
        if ((*it) == node_id) {
            hit = true;
            break;
        }
    }

    return hit;
}

bool node_request_service::check_nonce(const std::string& nonce) {
    if (nonce.empty()) {
        LOG_ERROR << "nonce is empty";
        return false;
    }

    if (m_nonceCache.contains(nonce)) {
        LOG_ERROR << "nonce is already used";
        return false;
    }
    else {
        m_nonceCache.insert(nonce, 1);
    }

    return true;
}

bool node_request_service::check_req_header_nonce(const std::string& nonce) {
    if (nonce.empty()) {
        LOG_ERROR << "header.nonce is empty";
        return false;
    }

    if (!util::check_id(nonce)) {
        LOG_ERROR << "header.nonce check_id failed";
        return false;
    }

    if (m_nonceCache.contains(nonce)) {
        LOG_ERROR << "header.nonce is already used";
        return false;
    }
    else {
        m_nonceCache.insert(nonce, 1);
    }

    return true;
}

bool node_request_service::check_req_header(const std::shared_ptr<dbc::network::message> &msg) {
    if (!msg) {
        LOG_ERROR << "msg is nullptr";
        return false;
    }

    std::shared_ptr<dbc::network::msg_base> base = msg->content;
    if (!base) {
        LOG_ERROR << "msg.content is nullptr";
        return false;
    }

    if (base->header.session_id.empty()) {
        LOG_ERROR << "base->header.session_id is empty";
        return false;
    }

    if (!util::check_id(base->header.session_id)) {
        LOG_ERROR << "header.session_id check failed";
        return false;
    }

    if (base->header.path.empty()) {
        LOG_ERROR << "header.path size <= 0";
        return false;
    }

    if (base->header.exten_info.size() < 3) {
        LOG_ERROR << "header.exten_info size < 3";
        return false;
    }

    if (base->header.exten_info.count("pub_key") <= 0) {
        LOG_ERROR << "header.exten_info has no pub_key";
        return false;
    }

    return true;
}

template <typename T>
void send_response_json(const std::string& msg_name, const dbc::network::base_header& header,
                   const std::string& rsp_data) {
    std::shared_ptr<T> rsp_msg_content = std::make_shared<T>();
    if (rsp_msg_content == nullptr) return;

    // header
    rsp_msg_content->header.__set_magic(conf_manager::instance().get_net_flag());
    rsp_msg_content->header.__set_msg_name(msg_name);
    rsp_msg_content->header.__set_nonce(util::create_nonce());
    rsp_msg_content->header.__set_session_id(header.session_id);
    rsp_msg_content->header.__set_path(header.path);
    std::map<std::string, std::string> exten_info;
    std::string sign_message = rsp_msg_content->header.nonce + rsp_msg_content->header.session_id;
    std::string sign = util::sign(sign_message, conf_manager::instance().get_node_private_key());
    exten_info["sign"] = sign;
    exten_info["pub_key"] = conf_manager::instance().get_pub_key();
    rsp_msg_content->header.__set_exten_info(exten_info);
    // body
    rsp_msg_content->body.__set_data(rsp_data);

    std::shared_ptr<dbc::network::message> rsp_msg = std::make_shared<dbc::network::message>();
    rsp_msg->set_name(msg_name);
    rsp_msg->set_content(rsp_msg_content);
    dbc::network::connection_manager::instance().send_resp_message(rsp_msg);
}

template <typename T>
void send_response_ok(const std::string& msg_name, const dbc::network::base_header& header) {
    std::stringstream ss;
    ss << "{";
    ss << "\"result_code\":" << E_SUCCESS;
    ss << ", \"result_message\":" << "\"ok\"";
    ss << "}";

    const std::map<std::string, std::string>& mp = header.exten_info;
    auto it = mp.find("pub_key");
    if (it != mp.end()) {
        std::string pub_key = it->second;
        std::string priv_key = conf_manager::instance().get_priv_key();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) ss.str().c_str(), ss.str().size(), pub_key, priv_key);
            send_response_json<T>(msg_name, header, s_data);
        } else {
            LOG_ERROR << "pub_key or priv_key is empty";
        }
    } else {
        LOG_ERROR << "no pub_key";
    }
}

template <typename T>
void send_response_error(const std::string& msg_name, const dbc::network::base_header& header, int32_t result,
                         const std::string& result_msg) {
    std::stringstream ss;
    ss << "{";
    ss << "\"result_code\":" << result;
    ss << ", \"result_message\":" << "\"" << result_msg << "\"";
    ss << "}";

    const std::map<std::string, std::string>& mp = header.exten_info;
    auto it = mp.find("pub_key");
    if (it != mp.end()) {
        std::string pub_key = it->second;
        std::string priv_key = conf_manager::instance().get_priv_key();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) ss.str().c_str(), ss.str().size(), pub_key, priv_key);
            send_response_json<T>(msg_name, header, s_data);
        } else {
            LOG_ERROR << "pub_key or priv_key is empty";
        }
    } else {
        LOG_ERROR << "no pub_key";
    }
}

std::string node_request_service::request_machine_status() {
    httplib::SSLClient cli(conf_manager::instance().get_dbc_chain_domain(), 443);
    std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"onlineProfile_getMachineInfo", "params": [")"
            + conf_manager::instance().get_node_id() + R"("]})";
    std::shared_ptr<httplib::Response> resp = cli.Post("/", str_send, "application/json");
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

int64_t node_request_service::get_cur_block() {
    httplib::SSLClient cli(conf_manager::instance().get_dbc_chain_domain(), 443);
    std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"chain_getBlock", "params": []}")";
    std::shared_ptr<httplib::Response> resp = cli.Post("/", str_send, "application/json");
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

bool node_request_service::in_verify_time(const std::string &wallet) {
    int64_t cur_block = 0;
    cur_block = get_cur_block();

    httplib::SSLClient cli(conf_manager::instance().get_dbc_chain_domain(), 443);
    std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"onlineCommittee_getCommitteeOps", "params": [")"
            + wallet + R"(",")" + conf_manager::instance().get_node_id() + R"("]})";
    std::shared_ptr<httplib::Response> resp = cli.Post("/", str_send, "application/json");
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

        // 不是验证人
        if (v_verifyTime.Size() != 3) {
            LOG_ERROR << "verify_time's size != 3";
            return false;
        }

        // 是否在验证时间内
        int64_t v1 = v_verifyTime[0].GetInt64();
        int64_t v2 = v_verifyTime[1].GetInt64();
        int64_t v3 = v_verifyTime[2].GetInt64();

        if ((cur_block >= v1 && cur_block <= v1 + 480) ||
            (cur_block >= v2 && cur_block <= v2 + 480) ||
            (cur_block >= v3 && cur_block <= v3 + 480))
        {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

int64_t node_request_service::is_renter(const std::string &wallet) {
    int64_t cur_rent_end = 0;

    httplib::SSLClient cli(conf_manager::instance().get_dbc_chain_domain(), 443);
    std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"rentMachine_getRentOrder", "params": [")"
            + wallet + R"(",")" + conf_manager::instance().get_node_id() + R"("]})";
    std::shared_ptr<httplib::Response> resp = cli.Post("/", str_send, "application/json");
    if (resp != nullptr) {
        do {
            rapidjson::Document doc;
            doc.Parse(resp->body.c_str());
            if (!doc.IsObject()) break;
            if (!doc.HasMember("result")) break;

            const rapidjson::Value &v_result = doc["result"];
            if (!v_result.IsObject()) break;

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

void node_request_service::check_authority(const std::string& request_wallet, const std::string& session_id,
                                           const std::string& session_id_sign, AuthoriseResult& result) {
    std::string str_status = request_machine_status();
    if (str_status.empty()) {
        LOG_ERROR << "http request machine_status failed";
        result.success = false;
        return;
    }

    // 未验证
    if (str_status == "AddingCustomizeInfo" || str_status == "DistributingOrder" ||
        str_status == "CommitteeVerifying" || str_status == "CommitteeRefused") {
        result.machine_status = MACHINE_STATUS::MS_VERIFY;

        bool ret = in_verify_time(request_wallet);
        if (!ret) {
            result.success = false;
        } else {
            m_task_scheduler.DeleteOtherCheckTask(request_wallet);

            result.success = true;
            result.user_role = USER_ROLE::UR_VERIFIER;
            result.rent_wallet = request_wallet;
        }

        return;
    }
    // 验证完，已上线
    else if (str_status == "WaitingFulfill" || str_status == "Online") {
        result.success = false;
        result.machine_status = MACHINE_STATUS::MS_ONLINE;
        return;
    }
    // 租用中
    else if (str_status == "creating" || str_status == "rented") {
        m_task_scheduler.DeleteAllCheckTasks();

        result.machine_status = MACHINE_STATUS::MS_RENNTED;
        int64_t rent_end = is_renter(request_wallet);
        if (rent_end > 0) {
            result.success = true;
            result.user_role = USER_ROLE::UR_RENTER;
            result.rent_wallet = request_wallet;
            result.rent_end = rent_end;

            m_task_scheduler.CreateSessionId(request_wallet);
        } else {
            std::string rent_wallet = m_task_scheduler.CheckSessionId(session_id, session_id_sign);
            if (rent_wallet.empty()) {
                result.success = false;
            } else {
                int64_t rent_end2 = is_renter(rent_wallet);
                if (rent_end2 <= 0) {
                    result.success = false;
                } else {
                    result.success = true;
                    result.user_role = USER_ROLE::UR_RENTER;
                    result.rent_wallet = rent_wallet;
                    result.rent_end = rent_end2;
                }
            }
        }
    }
    // 未知状态
    else {
        return;
    }
}


void node_request_service::on_node_query_node_info_req(const std::shared_ptr<dbc::network::message> &msg) {
    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_query_node_info_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    if (!check_req_header_nonce(node_req_msg->header.nonce)) {
        LOG_ERROR << "request header.nonce check failed";
        return;
    }

    if (!check_req_header(msg)) {
        send_response_error<dbc::node_query_node_info_rsp>(NODE_QUERY_NODE_INFO_RSP, node_req_msg->header, E_DEFAULT, "request header check failed");
        LOG_ERROR << "request header check failed";
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        send_response_error<dbc::node_query_node_info_rsp>(NODE_QUERY_NODE_INFO_RSP, node_req_msg->header, E_DEFAULT, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_query_node_info_req_data> data = std::make_shared<dbc::node_query_node_info_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            //send_response_error<dbc::node_query_node_info_rsp>(NODE_QUERY_NODE_INFO_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error1");
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        //send_response_error<dbc::node_query_node_info_rsp>(NODE_QUERY_NODE_INFO_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    if (!check_nonce(nonce)) {
        send_response_error<dbc::node_query_node_info_rsp>(NODE_QUERY_NODE_INFO_RSP, node_req_msg->header, E_DEFAULT, "nonce check failed, nonce:" + nonce);
        LOG_ERROR << "nonce check failed, nonce: " << nonce;
        return;
    }

    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        send_response_error<dbc::node_query_node_info_rsp>(NODE_QUERY_NODE_INFO_RSP, node_req_msg->header, E_DEFAULT, "verify sign error");
        LOG_ERROR << "verify sign error";
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        query_node_info(node_req_msg->header, data);
    }
}

void node_request_service::query_node_info(const dbc::network::base_header& header,
                                           const std::shared_ptr<dbc::node_query_node_info_req_data>& data) {
    std::stringstream ss;
    ss << "{";
    ss << "\"result_code\":" << 0;
    ss << ",\"result_message\":" << "{";
    ss << "\"ip\":" << "\"" << SystemInfo::instance().get_publicip() << "\"";
    ss << ",\"os\":" << "\"" << SystemInfo::instance().get_osname() << "\"";
    cpu_info tmp_cpuinfo = SystemInfo::instance().get_cpuinfo();
    ss << ",\"cpu\":" << "{";
    ss << "\"type\":" << "\"" << tmp_cpuinfo.cpu_name << "\"";
    ss << ",\"hz\":" << "\"" << tmp_cpuinfo.mhz << "\"";
    ss << ",\"cores\":" << "\"" << tmp_cpuinfo.total_cores << "\"";
    ss << ",\"used_usage\":" << "\"" << (SystemInfo::instance().get_cpu_usage() * 100) << "%" << "\"";
    ss << "}";
    mem_info tmp_meminfo = SystemInfo::instance().get_meminfo();
    ss << ",\"mem\":" <<  "{";
    ss << "\"size\":" << "\"" << scale_size(tmp_meminfo.mem_total) << "\"";
    ss << ",\"free\":" << "\"" << scale_size(tmp_meminfo.mem_free) << "\"";
    ss << ",\"used_usage\":" << "\"" << (tmp_meminfo.mem_usage * 100) << "%" << "\"";
    ss << "}";
    disk_info tmp_diskinfo = SystemInfo::instance().get_diskinfo();
    ss << ",\"disk_system\":" << "{";
    ss << "\"type\":" << "\"" << (tmp_diskinfo.disk_type == DISK_SSD ? "SSD" : "HDD") << "\"";
    ss << ",\"size\":" << "\"" << g_disk_system_size << "G\"";
    ss << "}";
    ss << ",\"disk_data\":" << "{";
    ss << "\"type\":" << "\"" << (tmp_diskinfo.disk_type == DISK_SSD ? "SSD" : "HDD") << "\"";
    ss << ",\"size\":" << "\"" << (tmp_diskinfo.disk_total/1024L/1024L) << "G\"";
    ss << ",\"free\":" << "\"" << (tmp_diskinfo.disk_awalible/1024L/1024L) << "G\"";
    ss << ",\"used_usage\":" << "\"" << (tmp_diskinfo.disk_usage * 100) << "%" << "\"";
    ss << "}";
    std::vector<std::string> images;
    {
        boost::system::error_code error_code;
        if (boost::filesystem::is_regular_file("/data/ubuntu.qcow2", error_code) && !error_code) {
            images.push_back("ubuntu.qcow2");
        }
        if (boost::filesystem::is_regular_file("/data/ubuntu-2004.qcow2", error_code) && !error_code) {
            images.push_back("ubuntu-2004.qcow2");
        }
        if (boost::filesystem::is_regular_file("/data/win.qcow2", error_code) && !error_code) {
            images.push_back("win.qcow2");
        }
    }
    ss << ",\"images\":" << "[";
    for(int i = 0; i < images.size(); ++i) {
        ss << "\"" << images[i] << "\"";
        if (i < images.size() - 1)
            ss << ",";
    }
    ss << "]";
    /*
    int32_t count = m_task_scheduler.GetRunningTaskSize();
    std::string state;
    if (count <= 0) {
        state = "idle";
    }
    else {
        state = "busy(" + std::to_string(count) + ")";
    }
    ss << ",\"state\":" << "\"" << state << "\"";
    */
    ss << ",\"version\":" << "\"" << SystemInfo::instance().get_version() << "\"";
    ss << "}";
    ss << "}";

    const std::map<std::string, std::string>& mp = header.exten_info;
    auto it = mp.find("pub_key");
    if (it != mp.end()) {
        std::string pub_key = it->second;
        std::string priv_key = conf_manager::instance().get_priv_key();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) ss.str().c_str(), ss.str().size(), pub_key, priv_key);
            send_response_json<dbc::node_query_node_info_rsp>(NODE_QUERY_NODE_INFO_RSP, header, s_data);
        } else {
            LOG_ERROR << "pub_key or priv_key is empty";
        }
    } else {
        LOG_ERROR << "no pub_key";
    }
}


void node_request_service::on_node_list_task_req(const std::shared_ptr<dbc::network::message>& msg) {
    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_list_task_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    if (!check_req_header_nonce(node_req_msg->header.nonce)) {
        LOG_ERROR << "request header.nonce check failed";
        return;
    }

    if (!check_req_header(msg)) {
        send_response_error<dbc::node_list_task_rsp>(NODE_LIST_TASK_RSP, node_req_msg->header, E_DEFAULT, "request header check failed");
        LOG_ERROR << "request header check failed";
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        send_response_error<dbc::node_list_task_rsp>(NODE_LIST_TASK_RSP, node_req_msg->header, E_DEFAULT, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_list_task_req_data> data = std::make_shared<dbc::node_list_task_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            //send_response_error<dbc::node_list_task_rsp>(NODE_LIST_TASK_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error1");
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        //send_response_error<dbc::node_list_task_rsp>(NODE_LIST_TASK_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    if (!check_nonce(nonce)) {
        send_response_error<dbc::node_list_task_rsp>(NODE_LIST_TASK_RSP, node_req_msg->header, E_DEFAULT, "nonce check failed, nonce:" + nonce);
        LOG_ERROR << "nonce check failed, nonce: " << nonce;
        return;
    }

    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        send_response_error<dbc::node_list_task_rsp>(NODE_LIST_TASK_RSP, node_req_msg->header, E_DEFAULT, "verify sign error");
        LOG_ERROR << "verify sign error";
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        task_list(node_req_msg->header, data);
    }
}

void node_request_service::task_list(const dbc::network::base_header& header,
                                     const std::shared_ptr<dbc::node_list_task_req_data>& data) {
    AuthoriseResult result;
    check_authority(data->wallet, data->session_id, data->session_id_sign, result);

    if (!result.success) {
        LOG_INFO << "check authority failed ("
        << result.machine_status << ", " << result.user_role << ", "
        << result.rent_wallet << ", " << result.rent_end << ")";

        send_response_error<dbc::node_list_task_rsp>(NODE_LIST_TASK_RSP, header, E_DEFAULT, "check authority failed");
        return;
    }

    int ret_code = E_SUCCESS;
    std::string ret_msg = "ok";

    std::stringstream ss_tasks;
    if (data->task_id.empty()) {
        ss_tasks << "[";
        std::vector<std::shared_ptr<dbc::TaskInfo>> task_list;
        m_task_scheduler.ListAllTask(result.rent_wallet, task_list);
        int idx = 0;
        for (auto &task : task_list) {
            if (idx > 0)
                ss_tasks << ",";

            ss_tasks << "{";
            ss_tasks << "\"task_id\":" << "\"" << task->task_id << "\"";
            /*
            ss_tasks << ", \"ssh_ip\":" << "\"" << get_public_ip() << "\"";
            ss_tasks << ", \"ssh_port\":" << "\"" << task->ssh_port << "\"";
            ss_tasks << ", \"user_name\":" << "\"" << g_vm_login_username << "\"";
            ss_tasks << ", \"login_password\":" << "\"" << task->login_password << "\"";

            const TaskResourceManager& res_mgr = m_task_scheduler.GetTaskResourceManager();
            const std::map<int32_t, DeviceDisk>& task_disk_list = res_mgr.GetTaskDisk(task->task_id);
            int64_t disk_data = 0;
            if (!task_disk_list.empty()) {
                auto it_disk = task_disk_list.find(1);
                if (it_disk != task_disk_list.end())
                    disk_data = it_disk->second.total;
            }
            const DeviceCpu& task_cpu = res_mgr.GetTaskCpu(task->task_id);
            int32_t cpu_cores = task_cpu.sockets * task_cpu.cores_per_socket * task_cpu.threads_per_core;
            const std::map<std::string, DeviceGpu>& task_gpu_list = res_mgr.GetTaskGpu(task->task_id);
            uint32_t gpu_count = task_gpu_list.size();
            const DeviceMem& task_mem = res_mgr.GetTaskMem(task->task_id);
            int64_t mem_size = task_mem.total;

            ss_tasks << ", \"cpu_cores\":" << cpu_cores;
            ss_tasks << ", \"gpu_count\":" << gpu_count;
            ss_tasks << ", \"mem_size\":" << "\"" << size_to_string(mem_size, 1024L) << "\"";
            ss_tasks << ", \"disk_system\":" << "\"" << size_to_string(g_disk_system_size, 1024L * 1024L * 1024L) << "\"";
            ss_tasks << ", \"disk_data\":" << "\"" << size_to_string(disk_data, 1024L * 1024L) << "\"";
            */

            struct tm _tm{};
            time_t tt = task->create_time;
            localtime_r(&tt, &_tm);
            char buf[256] = {0};
            memset(buf, 0, sizeof(char) * 256);
            strftime(buf, sizeof(char) * 256, "%Y-%m-%d %H:%M:%S", &_tm);
            ss_tasks << ", \"create_time\":" << "\"" << buf << "\"";

            ss_tasks << ", \"status\":" << "\"" << task_status_string(m_task_scheduler.GetTaskStatus(task->task_id)) << "\"";
            ss_tasks << "}";

            idx++;
        }
        ss_tasks << "]";
    } else {
        auto task = m_task_scheduler.FindTask(result.rent_wallet, data->task_id);
        if (nullptr != task) {
            ss_tasks << "{";
            ss_tasks << "\"task_id\":" << "\"" << task->task_id << "\"";
            ss_tasks << ", \"ssh_ip\":" << "\"" << SystemInfo::instance().get_publicip() << "\"";
            ss_tasks << ", \"ssh_port\":" << "\"" << task->ssh_port << "\"";
            ss_tasks << ", \"user_name\":" << "\"" << g_vm_login_username << "\"";
            ss_tasks << ", \"login_password\":" << "\"" << task->login_password << "\"";

            const TaskResourceManager& res_mgr = m_task_scheduler.GetTaskResourceManager();
            const TaskResource& task_resource = res_mgr.GetTaskResource(data->task_id);
            uint64_t disk_data = task_resource.disks_data.begin()->second;
            int32_t cpu_cores = task_resource.total_cores();
            uint32_t gpu_count = task_resource.gpus.size();
            int64_t mem_size = task_resource.mem_size;

            ss_tasks << ", \"cpu_cores\":" << cpu_cores;
            ss_tasks << ", \"gpu_count\":" << gpu_count;
            ss_tasks << ", \"mem_size\":" << "\"" << size_to_string(mem_size, 1024L) << "\"";
            ss_tasks << ", \"disk_system\":" << "\"" << g_disk_system_size << "G\"";
            ss_tasks << ", \"disk_data\":" << "\"" << (disk_data / 1024L) << "G\"";

            struct tm _tm{};
            time_t tt = task->create_time;
            localtime_r(&tt, &_tm);
            char buf[256] = {0};
            memset(buf, 0, sizeof(char) * 256);
            strftime(buf, sizeof(char) * 256, "%Y-%m-%d %H:%M:%S", &_tm);
            ss_tasks << ", \"create_time\":" << "\"" << buf << "\"";

            ss_tasks << ", \"status\":" << "\"" << task_status_string(m_task_scheduler.GetTaskStatus(task->task_id)) << "\"";
            ss_tasks << "}";
        } else {
            ret_code = E_DEFAULT;
            ret_msg = "task_id not exist";
        }
    }

    std::stringstream ss;
    ss << "{";
    if (ret_code == E_SUCCESS) {
        ss << "\"status\":" << E_SUCCESS;
        ss << ", \"message\":" << ss_tasks.str();
    } else {
        ss << "\"status\":" << ret_code;
        ss << ", \"message\":" << "\"" << ret_msg << "\"";
    }
    ss << "}";

    const std::map<std::string, std::string>& mp = header.exten_info;
    auto it = mp.find("pub_key");
    if (it != mp.end()) {
        std::string pub_key = it->second;
        std::string priv_key = conf_manager::instance().get_priv_key();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) ss.str().c_str(), ss.str().size(), pub_key, priv_key);
            send_response_json<dbc::node_list_task_rsp>(NODE_LIST_TASK_RSP, header, s_data);
        } else {
            LOG_ERROR << "pub_key or priv_key is empty";
            send_response_error<dbc::node_list_task_rsp>(NODE_LIST_TASK_RSP, header, E_DEFAULT, "pub_key or priv_key is empty");
        }
    } else {
        LOG_ERROR << "request no pub_key";
        send_response_error<dbc::node_list_task_rsp>(NODE_LIST_TASK_RSP, header, E_DEFAULT, "request no pub_key");
    }
}


void node_request_service::on_node_create_task_req(const std::shared_ptr<dbc::network::message>& msg) {
    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_create_task_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    if (!check_req_header_nonce(node_req_msg->header.nonce)) {
        LOG_ERROR << "request header.nonce check failed";
        return;
    }

    if (!check_req_header(msg)) {
        send_response_error<dbc::node_create_task_rsp>(NODE_CREATE_TASK_RSP, node_req_msg->header, E_DEFAULT, "request header check failed");
        LOG_ERROR << "request header check failed";
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        send_response_error<dbc::node_create_task_rsp>(NODE_CREATE_TASK_RSP, node_req_msg->header, E_DEFAULT, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_create_task_req_data> data = std::make_shared<dbc::node_create_task_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            //send_response_error<dbc::node_create_task_rsp>(NODE_CREATE_TASK_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error1");
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        //send_response_error<dbc::node_create_task_rsp>(NODE_CREATE_TASK_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    if (!check_nonce(nonce)) {
        send_response_error<dbc::node_create_task_rsp>(NODE_CREATE_TASK_RSP, node_req_msg->header, E_DEFAULT, "nonce check failed, nonce:" + nonce);
        LOG_ERROR << "nonce check failed, nonce: " << nonce;
        return;
    }

    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        send_response_error<dbc::node_create_task_rsp>(NODE_CREATE_TASK_RSP, node_req_msg->header, E_DEFAULT, "verify sign error");
        LOG_ERROR << "verify sign error";
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        task_create(node_req_msg->header, data);
    }
}

static std::string generate_pwd() {
    char chr[] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G',
                   'H', 'I', 'J', 'K', 'L', 'M', 'N',
                   'O', 'P', 'Q', 'R', 'S', 'T',
                   'U', 'V', 'W', 'X', 'Y', 'Z',
                   'a', 'b', 'c', 'd', 'e', 'f', 'g',
                   'h', 'i', 'j', 'k', 'l', 'm', 'n',
                   'o', 'p', 'q', 'r', 's', 't',
                   'u', 'v', 'w', 'x', 'y', 'z',
                   '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };
    srand(time(NULL));
    std::string strpwd;
    int nlen = 10; //10位密码
    char buf[3] = { 0 };
    int idx0 = rand() % 52;
    sprintf(buf, "%c", chr[idx0]);
    strpwd.append(buf);

    int idx_0 = rand() % nlen;
    int idx_1 = rand() % nlen;
    int idx_2 = rand() % nlen;

    for (int i = 1; i < nlen; i++) {
        int idx;
        if (i == idx_0 || i == idx_1 || i == idx_2) {
            idx = rand() % 62;
        }
        else {
            idx = rand() % 62;
        }
        sprintf(buf, "%c", chr[idx]);
        strpwd.append(buf);
    }

    return strpwd;
}

void node_request_service::task_create(const dbc::network::base_header& header,
                                       const std::shared_ptr<dbc::node_create_task_req_data>& data) {
    AuthoriseResult result;
    check_authority(data->wallet, data->session_id, data->session_id_sign, result);

    if (!result.success) {
        LOG_INFO << "check authority failed ("
                 << result.machine_status << ", " << result.user_role << ", "
                 << result.rent_wallet << ", " << result.rent_end << ")";

        send_response_error<dbc::node_create_task_rsp>(NODE_CREATE_TASK_RSP, header, E_DEFAULT, "check authority failed");
        return;
    }

    int ret_code = E_SUCCESS;
    std::string ret_msg = "ok";

    std::string task_id = util::create_task_id();
    if (result.user_role == USER_ROLE::UR_VERIFIER) {
        task_id = "vm_check_" + std::to_string(time(nullptr));
    }
    std::string login_password = generate_pwd();
    auto fresult = m_task_scheduler.CreateTask(result.rent_wallet, task_id, login_password, data->additional,
                                               result.rent_end, result.user_role);
    ret_code = std::get<0>(fresult);
    ret_msg = std::get<1>(fresult);

    std::stringstream ss;
    if (ret_code == E_SUCCESS) {
        ss << "{";
        ss << "\"status\":" << ret_code;
        ss << ", \"message\":" << "{";
        ss << "\"task_id\":" << "\"" << task_id << "\"";
        auto taskinfo = m_task_scheduler.FindTask(result.rent_wallet, task_id);
        struct tm _tm{};
        time_t tt = taskinfo == nullptr ? 0 : taskinfo->create_time;
        localtime_r(&tt, &_tm);
        char buf[256] = {0};
        memset(buf, 0, sizeof(char) * 256);
        strftime(buf, sizeof(char) * 256, "%Y-%m-%d %H:%M:%S", &_tm);
        ss << ", \"create_time\":" << "\"" << buf << "\"";
        ss << ", \"status\":" << "\"" << "creating" << "\"";
        ss << "}";
        ss << "}";
    } else {
        ss << "{";
        ss << "\"status\":" << ret_code;
        ss << ", \"message\":" << "\"" << ret_msg << "\"";
        ss << "}";
    }

    const std::map<std::string, std::string>& mp = header.exten_info;
    auto it = mp.find("pub_key");
    if (it != mp.end()) {
        std::string pub_key = it->second;
        std::string priv_key = conf_manager::instance().get_priv_key();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) ss.str().c_str(), ss.str().size(), pub_key, priv_key);
            send_response_json<dbc::node_create_task_rsp>(NODE_CREATE_TASK_RSP, header, s_data);
        } else {
            LOG_ERROR << "pub_key or priv_key is empty";
            send_response_error<dbc::node_create_task_rsp>(NODE_CREATE_TASK_RSP, header, E_DEFAULT, "pub_key or priv_key is empty");
        }
    } else {
        LOG_ERROR << "request no pub_key";
        send_response_error<dbc::node_create_task_rsp>(NODE_CREATE_TASK_RSP, header, E_DEFAULT, "request no pub_key");
    }
}


void node_request_service::on_node_start_task_req(const std::shared_ptr<dbc::network::message>& msg) {
    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_start_task_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    if (!check_req_header_nonce(node_req_msg->header.nonce)) {
        LOG_ERROR << "request header.nonce check failed";
        return;
    }

    if (!check_req_header(msg)) {
        send_response_error<dbc::node_start_task_rsp>(NODE_START_TASK_RSP, node_req_msg->header, E_DEFAULT, "request header check failed");
        LOG_ERROR << "request header check failed";
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        send_response_error<dbc::node_start_task_rsp>(NODE_START_TASK_RSP, node_req_msg->header, E_DEFAULT, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_start_task_req_data> data = std::make_shared<dbc::node_start_task_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            //send_response_error<dbc::node_start_task_rsp>(NODE_START_TASK_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error1");
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        //send_response_error<dbc::node_start_task_rsp>(NODE_START_TASK_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    if (!check_nonce(nonce)) {
        send_response_error<dbc::node_start_task_rsp>(NODE_START_TASK_RSP, node_req_msg->header, E_DEFAULT, "nonce check failed, nonce:" + nonce);
        LOG_ERROR << "nonce check failed, nonce: " << nonce;
        return;
    }

    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        send_response_error<dbc::node_start_task_rsp>(NODE_START_TASK_RSP, node_req_msg->header, E_DEFAULT, "verify sign error");
        LOG_ERROR << "verify sign error";
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        task_start(node_req_msg->header, data);
    }
}

void node_request_service::task_start(const dbc::network::base_header& header,
                                      const std::shared_ptr<dbc::node_start_task_req_data>& data) {
    AuthoriseResult result;
    check_authority(data->wallet, data->session_id, data->session_id_sign, result);

    if (!result.success) {
        LOG_INFO << "check authority failed ("
        << result.machine_status << ", " << result.user_role << ", "
        << result.rent_wallet << ", " << result.rent_end << ")";

        send_response_error<dbc::node_start_task_rsp>(NODE_START_TASK_RSP, header, E_DEFAULT, "check authority failed");
        return;
    }

    int ret_code = E_SUCCESS;
    std::string ret_msg = "ok";

    auto fresult = m_task_scheduler.StartTask(result.rent_wallet, data->task_id);
    ret_code = std::get<0>(fresult);
    ret_msg = std::get<1>(fresult);

    if (ret_code == E_SUCCESS) {
        send_response_ok<dbc::node_start_task_rsp>(NODE_START_TASK_RSP, header);
    } else {
        send_response_error<dbc::node_start_task_rsp>(NODE_START_TASK_RSP, header, ret_code, ret_msg);
    }
}


void node_request_service::on_node_stop_task_req(const std::shared_ptr<dbc::network::message>& msg) {
    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_stop_task_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    if (!check_req_header_nonce(node_req_msg->header.nonce)) {
        LOG_ERROR << "request header.nonce check failed";
        return;
    }

    if (!check_req_header(msg)) {
        send_response_error<dbc::node_stop_task_rsp>(NODE_STOP_TASK_RSP, node_req_msg->header, E_DEFAULT, "request header check failed");
        LOG_ERROR << "request header check failed";
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        send_response_error<dbc::node_stop_task_rsp>(NODE_STOP_TASK_RSP, node_req_msg->header, E_DEFAULT, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_stop_task_req_data> data = std::make_shared<dbc::node_stop_task_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            //send_response_error<dbc::node_stop_task_rsp>(NODE_STOP_TASK_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error1");
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        //send_response_error<dbc::node_stop_task_rsp>(NODE_STOP_TASK_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    if (!check_nonce(nonce)) {
        send_response_error<dbc::node_stop_task_rsp>(NODE_STOP_TASK_RSP, node_req_msg->header, E_DEFAULT, "nonce check failed, nonce:" + nonce);
        LOG_ERROR << "nonce check failed, nonce: " << nonce;
        return;
    }

    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        send_response_error<dbc::node_stop_task_rsp>(NODE_STOP_TASK_RSP, node_req_msg->header, E_DEFAULT, "verify sign error");
        LOG_ERROR << "verify sign error";
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        task_stop(node_req_msg->header, data);
    }
}

void node_request_service::task_stop(const dbc::network::base_header& header,
                                     const std::shared_ptr<dbc::node_stop_task_req_data>& data) {
    AuthoriseResult result;
    check_authority(data->wallet, data->session_id, data->session_id_sign, result);

    if (!result.success) {
        LOG_INFO << "check authority failed ("
        << result.machine_status << ", " << result.user_role << ", "
        << result.rent_wallet << ", " << result.rent_end << ")";

        send_response_error<dbc::node_stop_task_rsp>(NODE_STOP_TASK_RSP, header, E_DEFAULT, "check authority failed");
        return;
    }

    int ret_code = E_SUCCESS;
    std::string ret_msg = "ok";

    auto fresult = m_task_scheduler.StopTask(result.rent_wallet, data->task_id);
    ret_code = std::get<0>(fresult);
    ret_msg = std::get<1>(fresult);

    if (ret_code == E_SUCCESS) {
        send_response_ok<dbc::node_stop_task_rsp>(NODE_STOP_TASK_RSP, header);
    } else {
        send_response_error<dbc::node_stop_task_rsp>(NODE_STOP_TASK_RSP, header, ret_code, ret_msg);
    }
}


void node_request_service::on_node_restart_task_req(const std::shared_ptr<dbc::network::message>& msg) {
    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_restart_task_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    if (!check_req_header_nonce(node_req_msg->header.nonce)) {
        LOG_ERROR << "request header.nonce check failed";
        return;
    }

    if (!check_req_header(msg)) {
        send_response_error<dbc::node_restart_task_rsp>(NODE_RESTART_TASK_RSP, node_req_msg->header, E_DEFAULT, "request header check failed");
        LOG_ERROR << "request header check failed";
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        send_response_error<dbc::node_restart_task_rsp>(NODE_RESTART_TASK_RSP, node_req_msg->header, E_DEFAULT, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_restart_task_req_data> data = std::make_shared<dbc::node_restart_task_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            //send_response_error<dbc::node_restart_task_rsp>(NODE_RESTART_TASK_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error1");
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        //send_response_error<dbc::node_restart_task_rsp>(NODE_RESTART_TASK_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    if (!check_nonce(nonce)) {
        send_response_error<dbc::node_restart_task_rsp>(NODE_RESTART_TASK_RSP, node_req_msg->header, E_DEFAULT, "nonce check failed, nonce:" + nonce);
        LOG_ERROR << "nonce check failed, nonce:" << nonce;
        return;
    }

    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        send_response_error<dbc::node_restart_task_rsp>(NODE_RESTART_TASK_RSP, node_req_msg->header, E_DEFAULT, "verify sign error");
        LOG_ERROR << "verify sign error";
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        task_restart(node_req_msg->header, data);
    }
}

void node_request_service::task_restart(const dbc::network::base_header& header,
                                        const std::shared_ptr<dbc::node_restart_task_req_data>& data) {
    AuthoriseResult result;
    check_authority(data->wallet, data->session_id, data->session_id_sign, result);

    if (!result.success) {
        LOG_INFO << "check authority failed ("
        << result.machine_status << ", " << result.user_role << ", "
        << result.rent_wallet << ", " << result.rent_end << ")";

        send_response_error<dbc::node_restart_task_rsp>(NODE_RESTART_TASK_RSP, header, E_DEFAULT, "check authority failed");
        return;
    }

    int ret_code = E_SUCCESS;
    std::string ret_msg = "ok";

    auto fresult = m_task_scheduler.RestartTask(result.rent_wallet, data->task_id);
    ret_code = std::get<0>(fresult);
    ret_msg = std::get<1>(fresult);

    if (ret_code == E_SUCCESS) {
        send_response_ok<dbc::node_restart_task_rsp>(NODE_RESTART_TASK_RSP, header);
    } else {
        send_response_error<dbc::node_restart_task_rsp>(NODE_RESTART_TASK_RSP, header, ret_code, ret_msg);
    }
}


void node_request_service::on_node_reset_task_req(const std::shared_ptr<dbc::network::message>& msg) {
    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_reset_task_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    if (!check_req_header_nonce(node_req_msg->header.nonce)) {
        LOG_ERROR << "request header.nonce check failed";
        return;
    }

    if (!check_req_header(msg)) {
        send_response_error<dbc::node_reset_task_rsp>(NODE_RESET_TASK_RSP, node_req_msg->header, E_DEFAULT, "request header check failed");
        LOG_ERROR << "request header check failed";
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        send_response_error<dbc::node_reset_task_rsp>(NODE_RESET_TASK_RSP, node_req_msg->header, E_DEFAULT, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_reset_task_req_data> data = std::make_shared<dbc::node_reset_task_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            //send_response_error<dbc::node_reset_task_rsp>(NODE_RESET_TASK_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error1");
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        //send_response_error<dbc::node_reset_task_rsp>(NODE_RESET_TASK_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    if (!check_nonce(nonce)) {
        send_response_error<dbc::node_reset_task_rsp>(NODE_RESET_TASK_RSP, node_req_msg->header, E_DEFAULT, "nonce check failed, nonce:" + nonce);
        LOG_ERROR << "nonce check failed, nonce: " << nonce;
        return;
    }

    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        send_response_error<dbc::node_reset_task_rsp>(NODE_RESET_TASK_RSP, node_req_msg->header, E_DEFAULT, "verify sign error");
        LOG_ERROR << "verify sign error";
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        task_reset(node_req_msg->header, data);
    }
}

void node_request_service::task_reset(const dbc::network::base_header& header,
                                      const std::shared_ptr<dbc::node_reset_task_req_data>& data) {
    AuthoriseResult result;
    check_authority(data->wallet, data->session_id, data->session_id_sign, result);

    if (!result.success) {
        LOG_INFO << "check authority failed ("
        << result.machine_status << ", " << result.user_role << ", "
        << result.rent_wallet << ", " << result.rent_end << ")";

        send_response_error<dbc::node_reset_task_rsp>(NODE_RESET_TASK_RSP, header, E_DEFAULT, "check authority failed");
        return;
    }

    int ret_code = E_SUCCESS;
    std::string ret_msg = "ok";

    auto fresult = m_task_scheduler.ResetTask(result.rent_wallet, data->task_id);
    ret_code = std::get<0>(fresult);
    ret_msg = std::get<1>(fresult);

    if (ret_code == E_SUCCESS) {
        send_response_ok<dbc::node_reset_task_rsp>(NODE_RESET_TASK_RSP, header);
    } else {
        send_response_error<dbc::node_reset_task_rsp>(NODE_RESET_TASK_RSP, header, ret_code, ret_msg);
    }
}


void node_request_service::on_node_delete_task_req(const std::shared_ptr<dbc::network::message>& msg) {
    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_delete_task_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg id nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    if (!check_req_header_nonce(node_req_msg->header.nonce)) {
        LOG_ERROR << "request header.nonce check failed";
        return;
    }

    if (!check_req_header(msg)) {
        send_response_error<dbc::node_delete_task_rsp>(NODE_DELETE_TASK_RSP, node_req_msg->header, E_DEFAULT, "request header check failed");
        LOG_ERROR << "request header check failed";
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        send_response_error<dbc::node_delete_task_rsp>(NODE_DELETE_TASK_RSP, node_req_msg->header, E_DEFAULT, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_delete_task_req_data> data = std::make_shared<dbc::node_delete_task_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            //send_response_error<dbc::node_delete_task_rsp>(NODE_DELETE_TASK_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error1");
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        //send_response_error<dbc::node_delete_task_rsp>(NODE_DELETE_TASK_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    if (!check_nonce(nonce)) {
        send_response_error<dbc::node_delete_task_rsp>(NODE_DELETE_TASK_RSP, node_req_msg->header, E_DEFAULT, "nonce check failed, nonce:" + nonce);
        LOG_ERROR << "nonce check failed, nonce:" << nonce;
        return;
    }

    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        send_response_error<dbc::node_delete_task_rsp>(NODE_DELETE_TASK_RSP, node_req_msg->header, E_DEFAULT, "verify sign error");
        LOG_ERROR << "verify sign error";
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        task_delete(node_req_msg->header, data);
    }
}

void node_request_service::task_delete(const dbc::network::base_header& header,
                                       const std::shared_ptr<dbc::node_delete_task_req_data>& data) {
    AuthoriseResult result;
    check_authority(data->wallet, data->session_id, data->session_id_sign, result);

    if (!result.success) {
        LOG_INFO << "check authority failed ("
        << result.machine_status << ", " << result.user_role << ", "
        << result.rent_wallet << ", " << result.rent_end << ")";

        send_response_error<dbc::node_delete_task_rsp>(NODE_DELETE_TASK_RSP, header, E_DEFAULT, "check authority failed");
        return;
    }

    int ret_code = E_SUCCESS;
    std::string ret_msg = "ok";

    auto fresult = m_task_scheduler.DeleteTask(result.rent_wallet, data->task_id);
    ret_code = std::get<0>(fresult);
    ret_msg = std::get<1>(fresult);

    if (ret_code == E_SUCCESS) {
        send_response_ok<dbc::node_delete_task_rsp>(NODE_DELETE_TASK_RSP, header);
    } else {
        send_response_error<dbc::node_delete_task_rsp>(NODE_DELETE_TASK_RSP, header, ret_code, ret_msg);
    }
}


void node_request_service::on_node_task_logs_req(const std::shared_ptr<dbc::network::message>& msg) {
    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_task_logs_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    if (!check_req_header_nonce(node_req_msg->header.nonce)) {
        LOG_ERROR << "request header.nonce check failed";
        return;
    }

    if (!check_req_header(msg)) {
        send_response_error<dbc::node_task_logs_rsp>(NODE_TASK_LOGS_RSP, node_req_msg->header, E_DEFAULT, "request header check failed");
        LOG_ERROR << "request header check failed";
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        send_response_error<dbc::node_task_logs_rsp>(NODE_TASK_LOGS_RSP, node_req_msg->header, E_DEFAULT, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_task_logs_req_data> data = std::make_shared<dbc::node_task_logs_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            //send_response_error<dbc::node_task_logs_rsp>(NODE_TASK_LOGS_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error1");
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        //send_response_error<dbc::node_task_logs_rsp>(NODE_TASK_LOGS_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    if (!check_nonce(nonce)) {
        send_response_error<dbc::node_task_logs_rsp>(NODE_TASK_LOGS_RSP, node_req_msg->header, E_DEFAULT, "nonce check failed, nonce:" + nonce);
        LOG_ERROR << "nonce check failed, nonce:" << nonce;
        return;
    }

    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        send_response_error<dbc::node_task_logs_rsp>(NODE_TASK_LOGS_RSP, node_req_msg->header, E_DEFAULT, "verify sign error");
        LOG_ERROR << "verify sign error";
        return;
    }

    int16_t req_head_or_tail = data->head_or_tail;
    int32_t req_number_of_lines = data->number_of_lines;

    if (GET_LOG_HEAD != req_head_or_tail && GET_LOG_TAIL != req_head_or_tail) {
        send_response_error<dbc::node_task_logs_rsp>(NODE_TASK_LOGS_RSP, node_req_msg->header, E_DEFAULT, "req_head_or_tail is invalid:" + req_head_or_tail);
        LOG_ERROR << "req_head_or_tail is invalid:" << req_head_or_tail;
        return;
    }

    if (req_number_of_lines > MAX_NUMBER_OF_LINES || req_number_of_lines < 0) {
        send_response_error<dbc::node_task_logs_rsp>(NODE_TASK_LOGS_RSP, node_req_msg->header, E_DEFAULT, "req_number_of_lines is invalid:" + req_number_of_lines);
        LOG_ERROR << "req_number_of_lines is invalid:" << req_number_of_lines;
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        task_logs(node_req_msg->header, data);
    }
}

void node_request_service::task_logs(const dbc::network::base_header& header,
                                     const std::shared_ptr<dbc::node_task_logs_req_data>& data) {
    AuthoriseResult result;
    check_authority(data->wallet, data->session_id, data->session_id_sign, result);

    if (!result.success) {
        LOG_INFO << "check authority failed";
        send_response_error<dbc::node_task_logs_rsp>(NODE_TASK_LOGS_RSP, header, E_DEFAULT, "check authority failed");
        return;
    }

    int ret_code = E_SUCCESS;
    std::string ret_msg = "ok";

    int16_t head_or_tail = data->head_or_tail;
    int32_t number_of_lines = data->number_of_lines;
    std::string log_content;
    auto fresult = m_task_scheduler.GetTaskLog(data->task_id, (ETaskLogDirection) head_or_tail,
                                               number_of_lines, log_content);
    ret_code = std::get<0>(fresult);
    ret_msg = std::get<1>(fresult);

    if (ret_code == E_SUCCESS) {
        std::stringstream ss;
        ss << "{";
        ss << "\"status\":" << E_SUCCESS;
        ss << ", \"message\":" << "\"" << log_content << "\"";
        ss << "}";

        const std::map<std::string, std::string>& mp = header.exten_info;
        auto it = mp.find("pub_key");
        if (it != mp.end()) {
            std::string pub_key = it->second;
            std::string priv_key = conf_manager::instance().get_priv_key();

            if (!pub_key.empty() && !priv_key.empty()) {
                std::string s_data = encrypt_data((unsigned char*) ss.str().c_str(), ss.str().size(), pub_key, priv_key);
                send_response_json<dbc::node_task_logs_rsp>(NODE_TASK_LOGS_RSP, header, s_data);
            } else {
                LOG_ERROR << "pub_key or priv_key is empty";
                send_response_error<dbc::node_task_logs_rsp>(NODE_TASK_LOGS_RSP, header, E_DEFAULT, "pub_key or priv_key is empty");
            }
        } else {
            LOG_ERROR << "request no pub_key";
            send_response_error<dbc::node_task_logs_rsp>(NODE_TASK_LOGS_RSP, header, E_DEFAULT, "request no pub_key");
        }
    } else {
        send_response_error<dbc::node_task_logs_rsp>(NODE_TASK_LOGS_RSP, header, ret_code, ret_msg);
    }
}


void node_request_service::on_node_session_id_req(const std::shared_ptr<dbc::network::message> &msg) {
    auto node_req_msg = std::dynamic_pointer_cast<dbc::node_session_id_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg id nullptr";
        return;
    }

    if (!m_is_computing_node) {
        node_req_msg->header.path.push_back(conf_manager::instance().get_node_id());
        dbc::network::connection_manager::instance().broadcast_message(msg, msg->header.src_sid);
        return;
    }

    if (!check_req_header_nonce(node_req_msg->header.nonce)) {
        LOG_ERROR << "request header.nonce check failed";
        return;
    }

    if (!check_req_header(msg)) {
        send_response_error<dbc::node_session_id_rsp>(NODE_SESSION_ID_RSP, node_req_msg->header, E_DEFAULT, "request header check failed");
        LOG_ERROR << "request header check failed";
        return;
    }

    // decrypt
    std::string pub_key = node_req_msg->header.exten_info["pub_key"];
    std::string priv_key = conf_manager::instance().get_priv_key();
    if (pub_key.empty() || priv_key.empty()) {
        send_response_error<dbc::node_session_id_rsp>(NODE_SESSION_ID_RSP, node_req_msg->header, E_DEFAULT, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::shared_ptr<dbc::node_session_id_req_data> data = std::make_shared<dbc::node_session_id_req_data>();
    try {
        std::string ori_message;
        bool succ = decrypt_data(node_req_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            //send_response_error<dbc::node_session_id_rsp>(NODE_SESSION_ID_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error1");
            LOG_ERROR << "req decrypt error1";
            return;
        }

        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(ori_message.c_str(), ori_message.size());
        dbc::network::binary_protocol proto(task_buf.get());
        data->read(&proto);
    } catch (std::exception &e) {
        //send_response_error<dbc::node_session_id_rsp>(NODE_SESSION_ID_RSP, node_req_msg->header, E_DEFAULT, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    std::string nonce = node_req_msg->header.exten_info["nonce"];
    if (!check_nonce(nonce)) {
        send_response_error<dbc::node_session_id_rsp>(NODE_SESSION_ID_RSP, node_req_msg->header, E_DEFAULT, "nonce check failed, nonce:" + nonce);
        LOG_ERROR << "nonce check failed, nonce:" << nonce;
        return;
    }

    std::string nonce_sign = node_req_msg->header.exten_info["sign"];
    if (!util::verify_sign(nonce_sign, nonce, data->wallet)) {
        send_response_error<dbc::node_session_id_rsp>(NODE_SESSION_ID_RSP, node_req_msg->header, E_DEFAULT, "verify sign error");
        LOG_ERROR << "verify sign error";
        return;
    }

    std::vector<std::string> req_peer_nodes = data->peer_nodes_list;
    bool hit_self = hit_node(req_peer_nodes, conf_manager::instance().get_node_id());
    if (hit_self) {
        node_session_id(node_req_msg->header, data);
    }
}

void node_request_service::node_session_id(const dbc::network::base_header &header,
                                           const std::shared_ptr<dbc::node_session_id_req_data> &data) {
    AuthoriseResult result;
    check_authority(data->wallet, data->session_id, data->session_id_sign, result);

    if (!result.success) {
        LOG_INFO << "check authority failed ("
        << result.machine_status << ", " << result.user_role << ", "
        << result.rent_wallet << ", " << result.rent_end << ")";

        send_response_error<dbc::node_session_id_rsp>(NODE_SESSION_ID_RSP, header, E_DEFAULT, "check authority failed");
        return;
    }

    if (result.machine_status == MACHINE_STATUS::MS_RENNTED && result.user_role == USER_ROLE::UR_RENTER) {
        std::string session_id = m_task_scheduler.GetSessionId(result.rent_wallet);
        if (session_id.empty()) {
            send_response_error<dbc::node_session_id_rsp>(NODE_SESSION_ID_RSP, header, E_DEFAULT, "no session id");
        } else {
            std::stringstream ss;
            ss << "{";
            ss << "\"status\":" << E_SUCCESS;
            ss << ", \"message\":" << "\"" << session_id << "\"";
            ss << "}";

            const std::map<std::string, std::string>& mp = header.exten_info;
            auto it = mp.find("pub_key");
            if (it != mp.end()) {
                std::string pub_key = it->second;
                std::string priv_key = conf_manager::instance().get_priv_key();

                if (!pub_key.empty() && !priv_key.empty()) {
                    std::string s_data = encrypt_data((unsigned char*) ss.str().c_str(), ss.str().size(), pub_key, priv_key);
                    send_response_json<dbc::node_session_id_rsp>(NODE_SESSION_ID_RSP, header, s_data);
                } else {
                    LOG_ERROR << "pub_key or priv_key is empty";
                    send_response_error<dbc::node_session_id_rsp>(NODE_SESSION_ID_RSP, header, E_DEFAULT, "pub_key or priv_key is empty");
                }
            } else {
                LOG_ERROR << "request no pub_key";
                send_response_error<dbc::node_session_id_rsp>(NODE_SESSION_ID_RSP, header, E_DEFAULT, "request no pub_key");
            }
        }
    } else {
        LOG_INFO << "check authority failed";
        send_response_error<dbc::node_session_id_rsp>(NODE_SESSION_ID_RSP, header, E_DEFAULT, "check authority failed");
    }
}


void node_request_service::on_training_task_timer(const std::shared_ptr<core_timer>& timer) {
    m_task_scheduler.ProcessTask();
}

void node_request_service::on_prune_task_timer(const std::shared_ptr<core_timer>& timer) {
    m_task_scheduler.PruneTask();
}

void node_request_service::on_timer_service_broadcast(const std::shared_ptr<core_timer>& timer)
{
    auto s_map_size = service_info_collection::instance().size();
    if (s_map_size == 0) {
        return;
    }

    if (m_is_computing_node) {
        /*
        int32_t count = m_task_scheduler.GetRunningTaskSize();
        std::string state;
        if (count <= 0) {
            state = "idle";
        } else {
            state = "busy(" + std::to_string(count) + ")";
        }

        service_info_collection::instance().update(conf_manager::instance().get_node_id(), "state", state);
        */
        service_info_collection::instance().update(conf_manager::instance().get_node_id(), "version", SystemInfo::instance().get_version());
        service_info_collection::instance().update_own_node_time_stamp(conf_manager::instance().get_node_id());
    }

    service_info_collection::instance().remove_unlived_nodes(conf_manager::instance().get_timer_service_list_expired_in_second());

    auto service_info_map = service_info_collection::instance().get_change_set();
    if(!service_info_map.empty()) {
        auto service_broadcast_req = create_service_broadcast_req_msg(service_info_map);
        if (service_broadcast_req != nullptr) {
            dbc::network::connection_manager::instance().broadcast_message(service_broadcast_req);
        }
    }
}

std::shared_ptr<dbc::network::message> node_request_service::create_service_broadcast_req_msg(const service_info_map& mp) {
    auto req_content = std::make_shared<dbc::service_broadcast_req>();
    // header
    req_content->header.__set_magic(conf_manager::instance().get_net_flag());
    req_content->header.__set_msg_name(SERVICE_BROADCAST_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(util::create_session_id());
    std::vector<std::string> path;
    path.push_back(conf_manager::instance().get_node_id());
    req_content->header.__set_path(path);
    std::map<std::string, std::string> exten_info;
    std::string sign_message = req_content->header.nonce + req_content->header.session_id;
    std::string signature = util::sign(sign_message, conf_manager::instance().get_node_private_key());
    if (signature.empty())  return nullptr;
    exten_info["sign"] = signature;
    exten_info["node_id"] = conf_manager::instance().get_node_id();
    exten_info["pub_key"] = conf_manager::instance().get_pub_key();
    req_content->header.__set_exten_info(exten_info);
    // body
    req_content->body.__set_node_service_info_map(mp);

    auto req_msg = std::make_shared<dbc::network::message>();
    req_msg->set_name(SERVICE_BROADCAST_REQ);
    req_msg->set_content(req_content);
    return req_msg;
}

void node_request_service::on_net_service_broadcast_req(const std::shared_ptr<dbc::network::message> &msg) {
    auto node_req_msg = std::dynamic_pointer_cast<dbc::service_broadcast_req>(msg->get_content());
    if (node_req_msg == nullptr) {
        LOG_ERROR << "node_req_msg is nullptr";
        return;
    }

    if (!check_req_header_nonce(node_req_msg->header.nonce)) {
        LOG_ERROR << "request header.nonce check failed";
        return;
    }

    if (!check_req_header(msg)) {
        LOG_ERROR << "req header check failed";
        return;
    }

    std::string sign_msg = node_req_msg->header.nonce + node_req_msg->header.session_id;
    if (!util::verify_sign(node_req_msg->header.exten_info["sign"], sign_msg, node_req_msg->header.exten_info["node_id"])) {
        LOG_ERROR << "verify sign error." << node_req_msg->header.exten_info["origin_id"];
        return;
    }

    service_info_map mp = node_req_msg->body.node_service_info_map;

    service_info_collection::instance().add(mp);
}

std::string node_request_service::format_logs(const std::string& raw_logs, uint16_t max_lines) {
    //docker logs has special format with each line of log:
    // 0x01 0x00  0x00 0x00 0x00 0x00 0x00 0x38
    //we should remove it
    //and ends with 0x30 0x0d 0x0a
    max_lines = (max_lines == 0) ? MAX_NUMBER_OF_LINES : max_lines;
    size_t size = raw_logs.size();
    std::vector<unsigned char> log_vector(size);

    int push_char_count = 0;
    const char* p = raw_logs.c_str();

    uint16_t line_count = 1;

    for (size_t i = 0; i < size;) {
        //0x30 0x0d 0x0a
        if ((i + 2 < size)
            && (0x30 == *p)
            && (0x0d == *(p + 1))
            && (0x0a == *(p + 2))) {
            break;
        }

        if (max_lines != 0 && line_count > max_lines) {
            break;
        }

        //skip: 0x01 0x00  0x00 0x00 0x00 0x00 0x00 0x38
        if ((i + 7 < size)
            && ((0x01 == *p) || (0x02 == *p))
            && (0x00 == *(p + 1))
            && (0x00 == *(p + 2))
            && (0x00 == *(p + 3))
            && (0x00 == *(p + 4))
            && (0x00 == *(p + 5))) {
            i += 8;
            p += 8;
            continue;
        }

        log_vector[push_char_count] = *p++;

        if (log_vector[push_char_count] == '\n') {
            line_count++;
        }

        ++push_char_count;
        i++;
    }

    std::string formatted_str;
    formatted_str.reserve(push_char_count);

    int i = 0;
    while (i < push_char_count) {
        formatted_str += log_vector[i++];
    }

    return formatted_str;
}
