#include "rest_api_service.h"
#include <boost/exception/all.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>
#include "rapidjson/rapidjson.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"
#include "rapidjson/error/error.h"
#include "util/system_info.h"
#include "log/log.h"
#include "timer/time_tick_notification.h"
#include "message/protocol_coder/matrix_coder.h"
#include "message/message_id.h"
#include "service_info/ServiceInfoManager.h"
#include "service/peer_request_service/p2p_net_service.h"
#include "service/peer_request_service/p2p_lan_service.h"
#include "task/detail/VxlanManager.h"
#include "network/connection_manager.h"
#include "config/conf_manager.h"
#include "task/detail/image/ImageManager.h"
#include "config/BareMetalNodeManager.h"
#include "server/server.h"

#define HTTP_REQUEST_KEY    "hreq_context"

ERRCODE rest_api_service::init() {
    service_module::init();

    const network::http_path_handler uri_prefixes[] = {
            {"/",             true,  std::bind(&rest_api_service::rest_client_version, this, std::placeholders::_1, std::placeholders::_2)},
            {"/tasks",        false, std::bind(&rest_api_service::rest_task, this, std::placeholders::_1, std::placeholders::_2)},
            {"/images",       false, std::bind(&rest_api_service::rest_images, this, std::placeholders::_1, std::placeholders::_2)},
			{"/snapshot",     false, std::bind(&rest_api_service::rest_snapshot, this, std::placeholders::_1, std::placeholders::_2)},
			{"/disk",         false, std::bind(&rest_api_service::rest_disk, this, std::placeholders::_1, std::placeholders::_2)},
            {"/mining_nodes", false, std::bind(&rest_api_service::rest_mining_nodes, this, std::placeholders::_1, std::placeholders::_2)},
            {"/peers",        false, std::bind(&rest_api_service::rest_peers, this, std::placeholders::_1, std::placeholders::_2)},
            {"/stat",         false, std::bind(&rest_api_service::rest_stat, this, std::placeholders::_1, std::placeholders::_2)},
            {"/config",       false, std::bind(&rest_api_service::rest_config, this, std::placeholders::_1, std::placeholders::_2)},
            {"/monitor",      false, std::bind(&rest_api_service::rest_monitor, this, std::placeholders::_1, std::placeholders::_2)},
            {"/lan",          false, std::bind(&rest_api_service::rest_lan, this, std::placeholders::_1, std::placeholders::_2)},
            {"/bare_metal",   false, std::bind(&rest_api_service::rest_bare_metal, this, std::placeholders::_1, std::placeholders::_2)},
            {"/rent_orders",  false, std::bind(&rest_api_service::rest_rent_orders, this, std::placeholders::_1, std::placeholders::_2)}
    };

    for (const auto &uri_prefixe : uri_prefixes) {
        m_path_handlers.emplace_back(REST_API_URI + uri_prefixe.m_prefix, uri_prefixe.m_exact_match, uri_prefixe.m_handler);
    }

    const network::response_msg_handler rsp_handlers[] = {
            {     NODE_CREATE_TASK_RSP, std::bind(&rest_api_service::on_node_create_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {      NODE_START_TASK_RSP, std::bind(&rest_api_service::on_node_start_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {   NODE_SHUTDOWN_TASK_RSP, std::bind(&rest_api_service::on_node_shutdown_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
			{   NODE_POWEROFF_TASK_RSP, std::bind(&rest_api_service::on_node_poweroff_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
			{       NODE_STOP_TASK_RSP, std::bind(&rest_api_service::on_node_stop_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {    NODE_RESTART_TASK_RSP, std::bind(&rest_api_service::on_node_restart_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {      NODE_RESET_TASK_RSP, std::bind(&rest_api_service::on_node_reset_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {     NODE_DELETE_TASK_RSP, std::bind(&rest_api_service::on_node_delete_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {       NODE_TASK_LOGS_RSP, std::bind(&rest_api_service::on_node_task_logs_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {       NODE_LIST_TASK_RSP, std::bind(&rest_api_service::on_node_list_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {     NODE_MODIFY_TASK_RSP, std::bind(&rest_api_service::on_node_modify_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {     NODE_PASSWD_TASK_RSP, std::bind(&rest_api_service::on_node_passwd_task_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            { NODE_QUERY_NODE_INFO_RSP, std::bind(&rest_api_service::on_node_query_node_info_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            { QUERY_NODE_RENT_ORDERS_RSP, std::bind(&rest_api_service::on_query_node_rent_orders_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {      NODE_SESSION_ID_RSP, std::bind(&rest_api_service::on_node_session_id_rsp, this, std::placeholders::_1, std::placeholders::_2) },
			{     NODE_FREE_MEMORY_RSP, std::bind(&rest_api_service::on_node_free_memory_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {     NODE_LIST_IMAGES_RSP, std::bind(&rest_api_service::on_node_list_images_rsp, this, std::placeholders::_1, std::placeholders::_2) },
			{              NODE_DOWNLOAD_IMAGE_RSP, std::bind(&rest_api_service::on_node_download_image_rsp, this, std::placeholders::_1, std::placeholders::_2) },
			{     NODE_DOWNLOAD_IMAGE_PROGRESS_RSP, std::bind(&rest_api_service::on_node_download_image_progress_rsp, this, std::placeholders::_1, std::placeholders::_2) },
			{         NODE_STOP_DOWNLOAD_IMAGE_RSP, std::bind(&rest_api_service::on_node_stop_download_image_rsp, this, std::placeholders::_1, std::placeholders::_2) },
			{                NODE_UPLOAD_IMAGE_RSP, std::bind(&rest_api_service::on_node_upload_image_rsp, this, std::placeholders::_1, std::placeholders::_2) },
			{       NODE_UPLOAD_IMAGE_PROGRESS_RSP, std::bind(&rest_api_service::on_node_upload_image_progress_rsp, this, std::placeholders::_1, std::placeholders::_2) },
			{           NODE_STOP_UPLOAD_IMAGE_RSP, std::bind(&rest_api_service::on_node_stop_upload_image_rsp, this, std::placeholders::_1, std::placeholders::_2) },
			{                NODE_DELETE_IMAGE_RSP, std::bind(&rest_api_service::on_node_delete_image_rsp, this, std::placeholders::_1, std::placeholders::_2) },
		    {   NODE_LIST_DISK_RSP, std::bind(&rest_api_service::on_node_list_disk_rsp, this, std::placeholders::_1, std::placeholders::_2) },
			{ NODE_RESIZE_DISK_RSP, std::bind(&rest_api_service::on_node_resize_disk_rsp, this, std::placeholders::_1, std::placeholders::_2) },
			{    NODE_ADD_DISK_RSP, std::bind(&rest_api_service::on_node_add_disk_rsp, this, std::placeholders::_1, std::placeholders::_2) },
			{ NODE_DELETE_DISK_RSP, std::bind(&rest_api_service::on_node_delete_disk_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {   NODE_LIST_SNAPSHOT_RSP, std::bind(&rest_api_service::on_node_list_snapshot_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            { NODE_CREATE_SNAPSHOT_RSP, std::bind(&rest_api_service::on_node_create_snapshot_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            { NODE_DELETE_SNAPSHOT_RSP, std::bind(&rest_api_service::on_node_delete_snapshot_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            { NODE_LIST_MONITOR_SERVER_RSP, std::bind(&rest_api_service::on_node_list_monitor_server_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {  NODE_SET_MONITOR_SERVER_RSP, std::bind(&rest_api_service::on_node_set_monitor_server_rsp, this, std::placeholders::_1, std::placeholders::_2), },
            {        NODE_LIST_LAN_RSP, std::bind(&rest_api_service::on_node_list_lan_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {      NODE_CREATE_LAN_RSP, std::bind(&rest_api_service::on_node_create_lan_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            {      NODE_DELETE_LAN_RSP, std::bind(&rest_api_service::on_node_delete_lan_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            { NODE_LIST_BARE_METAL_RSP, std::bind(&rest_api_service::on_node_list_bare_metal_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            { NODE_ADD_BARE_METAL_RSP, std::bind(&rest_api_service::on_node_add_bare_metal_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            { NODE_DELETE_BARE_METAL_RSP, std::bind(&rest_api_service::on_node_delete_bare_metal_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            { NODE_BARE_METAL_POWER_RSP, std::bind(&rest_api_service::on_node_bare_metal_power_rsp, this, std::placeholders::_1, std::placeholders::_2) },
            { NODE_BARE_METAL_BOOTDEV_RSP, std::bind(&rest_api_service::on_node_bare_metal_bootdev_rsp, this, std::placeholders::_1, std::placeholders::_2) }
    };

    for (const auto &rsp_handler : rsp_handlers) {
        std::string name = rsp_handler.name;
        m_rsp_handlers[name] = rsp_handler.handler;
    }

    return ERR_SUCCESS;
}

void rest_api_service::exit() {
    service_module::exit();
}

void rest_api_service::init_timer() {

}

void rest_api_service::init_invoker() {
    reg_msg_handle(NODE_CREATE_TASK_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_START_TASK_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_SHUTDOWN_TASK_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
	reg_msg_handle(NODE_POWEROFF_TASK_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
	reg_msg_handle(NODE_STOP_TASK_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_RESTART_TASK_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_RESET_TASK_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_DELETE_TASK_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_TASK_LOGS_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_LIST_TASK_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_MODIFY_TASK_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_PASSWD_TASK_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_QUERY_NODE_INFO_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(QUERY_NODE_RENT_ORDERS_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_SESSION_ID_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
	reg_msg_handle(NODE_FREE_MEMORY_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
	reg_msg_handle(NODE_LIST_IMAGES_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
	reg_msg_handle(NODE_DOWNLOAD_IMAGE_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
	reg_msg_handle(NODE_DOWNLOAD_IMAGE_PROGRESS_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
	reg_msg_handle(NODE_STOP_DOWNLOAD_IMAGE_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
	reg_msg_handle(NODE_UPLOAD_IMAGE_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
	reg_msg_handle(NODE_UPLOAD_IMAGE_PROGRESS_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
	reg_msg_handle(NODE_STOP_UPLOAD_IMAGE_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
	reg_msg_handle(NODE_DELETE_IMAGE_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
	reg_msg_handle(NODE_LIST_DISK_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
	reg_msg_handle(NODE_RESIZE_DISK_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
	reg_msg_handle(NODE_ADD_DISK_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
	reg_msg_handle(NODE_DELETE_DISK_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_LIST_SNAPSHOT_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_CREATE_SNAPSHOT_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_DELETE_SNAPSHOT_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_LIST_MONITOR_SERVER_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_SET_MONITOR_SERVER_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_LIST_LAN_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_CREATE_LAN_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_DELETE_LAN_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_LIST_BARE_METAL_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_ADD_BARE_METAL_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_DELETE_BARE_METAL_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_BARE_METAL_POWER_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    reg_msg_handle(NODE_BARE_METAL_BOOTDEV_RSP, CALLBACK_1(rest_api_service::on_call_rsp_handler, this));
    //m_invokers[BINARY_FORWARD_MSG] = std::bind(&rest_api_service::on_binary_forward, this, std::placeholders::_1);
}

void rest_api_service::on_http_request_event(std::shared_ptr<network::http_request> &hreq) {
    std::string str_uri = hreq->get_uri();
    if (str_uri.substr(0, REST_API_URI.size()) != REST_API_URI) {
        LOG_ERROR << "request uri is invalid, uri:" << str_uri;
        hreq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "request uri is invalid, uri:" + str_uri);
        return;
    }

    std::string path;
    std::vector<network::http_path_handler>::const_iterator i = m_path_handlers.begin();
    std::vector<network::http_path_handler>::const_iterator iend = m_path_handlers.end();
    for (; i != iend; ++i) {
        bool match = false;
        if (i->m_exact_match) {
            match = (str_uri == i->m_prefix);
        } else {
            match = (str_uri.substr(0, i->m_prefix.size()) == i->m_prefix);
        }

        if (match) {
            path = str_uri.substr(i->m_prefix.size());
            break;
        }
    }

    if (i == iend) {
        LOG_ERROR << "not support uri:" << str_uri;
        hreq->reply_comm_rest_err(HTTP_NOTFOUND, RPC_METHOD_NOT_FOUND, "not support uri:" + str_uri);
    } else {
        i->m_handler(hreq, path);
    }
}

bool rest_api_service::check_rsp_header(const std::shared_ptr<network::message> &rsp_msg) {
    if (!rsp_msg) {
        LOG_ERROR << "msg is nullptr";
        return false;
    }

    std::shared_ptr<network::msg_base> base = rsp_msg->content;
    if (!base) {
        LOG_ERROR << "msg.containt is nullptr";
        return false;
    }

    if (base->header.nonce.empty()) {
        LOG_ERROR << "header.nonce is empty";
        return false;
    }

    if (!util::check_id(base->header.nonce)) {
        LOG_ERROR << "header.nonce check_id failed";
        return false;
    }

    if (m_nonce_filter.contains(base->header.nonce)) {
        LOG_ERROR << "header.nonce is already used";
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

    if (base->header.exten_info.size() < 2) {
        LOG_ERROR << "header.exten_info size < 2";
        return false;
    }

    if (base->header.exten_info.count("pub_key") <= 0) {
        LOG_ERROR << "header.extern_info has no pub_Key";
        return false;
    }

    return true;
}

void rest_api_service::on_call_rsp_handler(const std::shared_ptr<network::message> &rsp_msg) {
    if (rsp_msg == nullptr) {
        LOG_ERROR << "rsp_msg is nullptr";
        return;
    }

    auto node_rsp_msg = rsp_msg->content;
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::string &name = rsp_msg->get_name();
    const std::string &session_id = node_rsp_msg->header.session_id;

    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_DEBUG << "rsp name: " << name << ",but cannot find  session_id:" << session_id;
        network::connection_manager::instance().send_resp_message(rsp_msg);
        return;
    }

    try {
        variables_map &vm = session->get_context().get_args();
        if (0 == vm.count(HTTP_REQUEST_KEY)) {
            LOG_ERROR << "rsp name: " << name << ",session_id:" << session_id << ",but get null hreq_key";
            return;
        }

        auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
        if (nullptr == hreq_context) {
            LOG_ERROR << "rsp name: " << name << ",session_id:" << session_id << ",but get null hreq_context";
            return;
        }

        if (!check_rsp_header(rsp_msg)) {
            LOG_ERROR << "rsp header check failed";
            return;
        }

        std::string sign_msg = node_rsp_msg->header.nonce + node_rsp_msg->header.session_id;
        if (!util::verify_sign(node_rsp_msg->header.exten_info["sign"], sign_msg, hreq_context->peer_node_id)) {
            LOG_ERROR << "rsp name: " << name << " verify sign failed peer_node_id:" << hreq_context->peer_node_id;
            return;
        }

        auto it = m_rsp_handlers.find(name);
        if (it == m_rsp_handlers.end()) {
            LOG_ERROR << "rsp name: " << name << ",session_id:" << session_id << ",but get null rsp_handler";
            return;
        }

        LOG_INFO << "rsp name: " << name << ",session_id:" << session_id << ", and call rsp_handler";

        auto func = it->second;
        func(hreq_context, rsp_msg);

        remove_timer(session->get_timer_id());
        session->clear();
        remove_session(session_id);
    } catch (std::exception &e) {
        LOG_ERROR << e.what();
    }
}

int32_t rest_api_service::create_request_session(uint32_t timer_id,
                                              const std::shared_ptr<network::http_request>& hreq,
                                              const std::shared_ptr<network::message>& req_msg,
                                              const std::string& session_id, const std::string& peer_node_id) {
    std::string str_uri = hreq->get_uri();

    if (get_session_count() >= MAX_SESSION_COUNT) {
        LOG_ERROR << "session pool is full, uri:" << str_uri;
        return E_DEFAULT;
    }

    auto hreq_context = std::make_shared<network::http_request_context>();
    hreq_context->m_hreq = hreq;
    hreq_context->m_req_msg = req_msg;
    hreq_context->peer_node_id = peer_node_id;

    std::shared_ptr<service_session> session = std::make_shared<service_session>(timer_id, session_id);
    variable_value val;
    val.value() = hreq_context;
    session->get_context().add(HTTP_REQUEST_KEY, val);

    int32_t ret = this->add_session(session_id, session);
    if (ERR_SUCCESS != ret) {
        LOG_ERROR << "add_session failed, uri:" << str_uri;
        return E_DEFAULT;
    }

    return ERR_SUCCESS;
}

static bool parse_req_params(const rapidjson::Document &doc, req_body& httpbody, std::string& error) {
    // peer_nodes_list【必填】
    if (doc.HasMember("peer_nodes_list")) {
        if (doc["peer_nodes_list"].IsArray()) {
            uint32_t list_size = doc["peer_nodes_list"].Size();
            if (list_size > 0) {
                std::string nodeid = doc["peer_nodes_list"][0].GetString();
                httpbody.peer_nodes_list.push_back(nodeid);
            }
        } else {
            error = "peer_nodes_list is not array";
            return false;
        }
    } else {
        error = "has no peer_nodes_list";
        return false;
    }
    // additional【必填】
    if (doc.HasMember("additional")) {
        if (doc["additional"].IsObject()) {
            const rapidjson::Value &obj = doc["additional"];
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            obj.Accept(writer);
            httpbody.additional = buffer.GetString();
        } else {
            error = "additional is not object";
            return false;
        }
    } else {
        error = "has no additional";
        return false;
    }
    // wallet【可选】
    if (doc.HasMember("wallet")) {
        if (doc["wallet"].IsString()) {
            httpbody.wallet = doc["wallet"].GetString();
        } else {
            error = "wallet is not string";
            return false;
        }
    }
    // nonce【可选】
    if (doc.HasMember("nonce")) {
        if (doc["nonce"].IsString()) {
            httpbody.nonce = doc["nonce"].GetString();
        } else {
            error = "nonce is not string";
            return false;
        }
    }
    // sign【可选】
    if (doc.HasMember("sign")) {
        if (doc["sign"].IsString()) {
            httpbody.sign = doc["sign"].GetString();
        } else {
            error = "sign is not string";
            return false;
        }
    }
    // multisig_accounts【可选】
    if (doc.HasMember("multisig_accounts")) {
        const rapidjson::Value& v_multisig_accounts = doc["multisig_accounts"];
        if (v_multisig_accounts.IsObject()) {
            if (v_multisig_accounts.HasMember("wallets")) {
                const rapidjson::Value& v_wallets = v_multisig_accounts["wallets"];
                if (v_wallets.IsArray()) {
                    for (rapidjson::SizeType i = 0; i < v_wallets.Size(); i++) {
                        httpbody.multisig_accounts.wallets.emplace_back(v_wallets[i].GetString());
                    }
                } else {
                    error = "multisig_accounts member wallets is not array";
                    return false;
                }
            } else {
                error = "multisig_accounts has no member wallets";
                return false;
            }

            if (v_multisig_accounts.HasMember("threshold")) {
                const rapidjson::Value& v_threshold = v_multisig_accounts["threshold"];
                if (v_threshold.IsString()) {
                    httpbody.multisig_accounts.threshold = atoi(v_multisig_accounts["threshold"].GetString());
                } else {
                    error = "multisig_accounts member threshold is not string";
                    return false;
                }
            } else {
                error = "multisig_accounts has no member threshold";
                return false;
            }

            if (v_multisig_accounts.HasMember("signs")) {
                const rapidjson::Value& v_signs = v_multisig_accounts["signs"];
                if (v_signs.IsArray()) {
                    for (rapidjson::SizeType i = 0; i < v_signs.Size(); i++) {
                        const rapidjson::Value& vSign = v_signs[i];
                        req_sign_item item;
                        item.wallet = vSign["wallet"].GetString();
                        item.nonce = vSign["nonce"].GetString();
                        item.sign = vSign["sign"].GetString();
                        httpbody.multisig_accounts.signs.push_back(item);
                    }
                } else {
                    error = "multisig_accounts member signs is not array";
                    return false;
                }
            } else {
                error = "multisig_accounts has no member signs";
                return false;
            }
        } else {
            error = "multisig_accounts is not object";
            return false;
        }
    }
    // session_id【可选】
    if (doc.HasMember("session_id")) {
        if (doc["session_id"].IsString()) {
            httpbody.session_id = doc["session_id"].GetString();
        } else {
            error = "session_id is not string";
            return false;
        }
    }
    // session_id_sign【可选】
    if (doc.HasMember("session_id_sign")) {
        if (doc["session_id_sign"].IsString()) {
            httpbody.session_id_sign = doc["session_id_sign"].GetString();
        } else {
            error = "session_id_sign is not string";
            return false;
        }
    }
    // rent_order【可选】
    if (doc.HasMember("rent_order")) {
        if (doc["rent_order"].IsString()) {
            httpbody.rent_order = doc["rent_order"].GetString();
        } else {
            error = "rent_order is not string";
            return false;
        }
    }
    // pub_key
    httpbody.pub_key = ConfManager::instance().GetPubKey();
    if (httpbody.pub_key.empty()) {
        error = "pub_key is empty";
        return false;
    }

    return true;
}

static bool check_wallet_sign(const req_body &httpbody) {
    return !httpbody.wallet.empty() && !httpbody.nonce.empty() && !httpbody.sign.empty();
}

static bool check_multisig_wallets(const req_body &httpbody) {
    bool valid = true;
    if (httpbody.multisig_accounts.threshold > httpbody.multisig_accounts.wallets.size() ||
                          httpbody.multisig_accounts.threshold > httpbody.multisig_accounts.signs.size()) {
        valid = false;
    } else {
        for (auto &it: httpbody.multisig_accounts.signs) {
            if (httpbody.multisig_accounts.wallets.end() ==
                std::find(httpbody.multisig_accounts.wallets.begin(), httpbody.multisig_accounts.wallets.end(),
                          it.wallet)) {
                valid = false;
                break;
            }

            if (it.wallet.empty() || it.nonce.empty() || it.sign.empty()) {
                valid = false;
                break;
            }
        }
    }

    return valid;
}

static bool has_peer_nodeid(const req_body& httpbody) {
    return !httpbody.peer_nodes_list.empty() && !httpbody.peer_nodes_list[0].empty();
}

// /
void rest_api_service::rest_client_version(const std::shared_ptr<network::http_request>& httpReq,
                                           const std::string &path) {
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType &allocator = doc.GetAllocator();
    rapidjson::Value obj(rapidjson::kObjectType);
    obj.AddMember("client_version", STRING_REF(dbcversion()), allocator);
    httpReq->reply_comm_rest_succ(obj);
}

// /tasks
void rest_api_service::rest_task(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.empty()) {
        rest_list_task(httpReq, path);
        return;
    }

    if (path_list.size() == 1) {
        const std::string &first_param = path_list[0];
        // create
        if (first_param == "start") {
            rest_create_task(httpReq, path);
        } else {
            rest_list_task(httpReq, path);
        }
        return;
    }

    if (path_list.size() == 2) {
        const std::string &second_param = path_list[0];
        if (second_param == "start") {
            rest_start_task(httpReq, path);
        } else if (second_param == "restart") {
            rest_restart_task(httpReq, path);
        } else if (second_param == "shutdown") {
            rest_shutdown_task(httpReq, path);
        } else if (second_param == "poweroff") {
			rest_poweroff_task(httpReq, path);
        } else if (second_param == "stop") {
            rest_stop_task(httpReq, path);
        } else if (second_param == "reset") {
            rest_reset_task(httpReq, path);
        } else if (second_param == "delete") {
            rest_delete_task(httpReq, path);
        } else if (second_param == "logs") {
            rest_task_logs(httpReq, path);
        } else if (second_param == "modify") {
            rest_modify_task(httpReq, path);
        } else if (second_param == "passwd") {
            rest_passwd_task(httpReq, path);
        }
        return;
    }

    httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "invalid requests uri");
}

// list task
void rest_api_service::rest_list_task(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    req_body body;

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.empty()) {
        body.task_id = "";
    } else if (path_list.size() == 1) {
        body.task_id = path_list[0];
    } else {
        LOG_ERROR << "path_list's size > 1";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /tasks");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
            return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_list_task_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_LIST_TASK_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_list_task_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_list_task_req_msg(const std::string &head_session_id,
                                                                                       const req_body& body) {
    auto req_content = std::make_shared<dbc::node_list_task_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_LIST_TASK_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_list_task_req_data req_data;
    req_data.__set_task_id(body.task_id);
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_LIST_TASK_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_list_task_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
                                             const std::shared_ptr<network::message>& rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_list_task_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_list_task_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "list task timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// create task
void rest_api_service::rest_create_task(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    req_body body;

    // parse body
    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 1) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use: /tasks/start");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    // peer_nodeid
    if (!has_peer_nodeid(body)) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    // local area network
    // std::string network_name;
    // JSON_PARSE_STRING(v_additional, "network_name", network_name);
    // if (!network_name.empty()) {
    //     std::shared_ptr<dbc::networkInfo> info = VxlanManager::instance().GetNetwork(network_name);
    //     if (!info) {
    //         httpReq->reply_comm_rest_succ2("{\"errcode\": -1,\"message\": \"network name not existed\"}");
    //         return;
    //     }
    //     rapidjson::Document docAdditional;
    //     rapidjson::ParseResult ok = docAdditional.Parse(body.additional.c_str());
    //     if (!ok) {
    //         httpReq->reply_comm_rest_succ2("{\"errcode\": -1,\"message\": \"parse additional network failed\"}");
    //         return;
    //     }
    //     rapidjson::Document::AllocatorType &allocator = docAdditional.GetAllocator();
    //     // add network info node
    //     rapidjson::Value netInfoObj(rapidjson::kObjectType);//创建一个Object类型的元素
    //     netInfoObj.AddMember("bridge_name", STRING_REF(info->bridgeName), allocator);
    //     netInfoObj.AddMember("vxlan_name", STRING_REF(info->vxlanName), allocator);
    //     netInfoObj.AddMember("vxlan_vni", STRING_REF(info->vxlanVni), allocator);
    //     docAdditional.AddMember("network_info", netInfoObj, allocator);  //添加object到Document中
    //     docAdditional.AddMember("vxlan_vni", STRING_REF(info->vxlanVni), allocator);
    //     //生成字符串
    //     rapidjson::StringBuffer buffer;
    //     rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    //     docAdditional.Accept(writer);
    //     body.additional = buffer.GetString();
    // }

    // session_id wallet
    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "session_id and wallet all invalid");
            return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_create_task_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_CREATE_TASK_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_create_task_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_create_task_req_msg(const std::string &head_session_id, const req_body& body) {
    auto req_content = std::make_shared<dbc::node_create_task_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_CREATE_TASK_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    std::string newadditional = body.additional;
	auto service_info1 = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
	if (service_info1 != nullptr) {
		if (service_info1->kvs.count("version") > 0) {
			std::string strversion = service_info1->kvs["version"];
			if (strversion > "0.3.9.7") {
                util::replace(newadditional, "\"ubuntu\"", "\"linux\"");
                util::replace(newadditional, "\"win\"", "\"windows\"");
            }
            else {
				util::replace(newadditional, "\"linux\"", "\"ubuntu\"");
				util::replace(newadditional, "\"windows\"", "\"win\"");
            }
		}
	}

    dbc::node_create_task_req_data req_data;
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(newadditional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {

            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_CREATE_TASK_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_create_task_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
                                                  const std::shared_ptr<network::message>& rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_create_task_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_create_task_timer(const std::shared_ptr<core_timer>& timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "create task timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// start task
void rest_api_service::rest_start_task(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 2) {
        LOG_ERROR << "path_list's size != 2";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /tasks/start/<task_id>");
        return;
    }

    req_body body;

    body.task_id = path_list[1];
    if (body.task_id.empty()) {
        LOG_ERROR << "task_id is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "task_id is empty");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
            return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_start_task_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_START_TASK_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_start_task_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_start_task_req_msg(
    const std::string &head_session_id, const req_body& body) {
    auto req_content = std::make_shared<dbc::node_start_task_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_START_TASK_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_start_task_req_data req_data;
    req_data.__set_task_id(body.task_id);
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_START_TASK_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_start_task_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
                                              const std::shared_ptr<network::message>& rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_start_task_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_start_task_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "start task timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// shutdown task
void rest_api_service::rest_shutdown_task(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 2) {
        LOG_ERROR << "path_list's size != 2";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /tasks/shutdown/<task_id>");
        return;
    }

    req_body body;

    body.task_id = path_list[1];
    if (body.task_id.empty()) {
        LOG_ERROR << "task_id is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "task_id is empty");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
            return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_shutdown_task_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_SHUTDOWN_TASK_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_shutdown_task_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_shutdown_task_req_msg(const std::string &head_session_id,
                                                                                       const req_body& body) {
    // 创建 node_ 请求
    std::shared_ptr<dbc::node_shutdown_task_req> req_content = std::make_shared<dbc::node_shutdown_task_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_SHUTDOWN_TASK_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_shutdown_task_req_data req_data;
    req_data.__set_task_id(body.task_id);
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_SHUTDOWN_TASK_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_shutdown_task_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
                                             const std::shared_ptr<network::message>& rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_shutdown_task_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_shutdown_task_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "shutdown task timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// poweroff task
void rest_api_service::rest_poweroff_task(const std::shared_ptr<network::http_request>& httpReq, const std::string& path) {
	if (httpReq->get_request_method() != network::http_request::POST) {
		LOG_ERROR << "http request is not post";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
		return;
	}

	std::vector<std::string> path_list;
	util::split_path(path, path_list);
	if (path_list.size() != 2) {
		LOG_ERROR << "path_list's size != 2";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /tasks/stop/<task_id>");
		return;
	}

	req_body body;

	body.task_id = path_list[1];
	if (body.task_id.empty()) {
		LOG_ERROR << "task_id is empty";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "task_id is empty");
		return;
	}

	std::string s_body = httpReq->read_body();
	if (s_body.empty()) {
		LOG_ERROR << "http request body is empty";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
	if (!ok) {
		std::stringstream ss;
		ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
		LOG_ERROR << ss.str();
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
		return;
	}

	if (!doc.IsObject()) {
		LOG_ERROR << "invalid json";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
		return;
	}

	std::string strerror;
	if (!parse_req_params(doc, body, strerror)) {
		LOG_ERROR << "parse req params failed: " << strerror;
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
		return;
	}

	if (!has_peer_nodeid(body)) {
		LOG_ERROR << "peer_nodeid is empty";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
		return;
	}

	if (body.session_id.empty() || body.session_id_sign.empty()) {
		if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
			LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
			return;
		}
	}

	// node request
	std::string head_session_id = util::create_session_id();

	auto node_req_msg = create_node_poweroff_task_req_msg(head_session_id, body);
	if (nullptr == node_req_msg) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
		return;
	}

	// timer
	uint32_t timer_id = add_timer(NODE_POWEROFF_TASK_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
		CALLBACK_1(rest_api_service::on_node_poweroff_task_timer, this));
	if (INVALID_TIMER_ID == timer_id) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
		return;
	}

	// session
	if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
		remove_timer(timer_id);
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
		return;
	}

	// broadcast message
	if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
		return;
	}
}

std::shared_ptr<network::message> rest_api_service::create_node_poweroff_task_req_msg(const std::string& head_session_id,
	const req_body& body) {
	// 创建 node_ 请求
	std::shared_ptr<dbc::node_poweroff_task_req> req_content = std::make_shared<dbc::node_poweroff_task_req>();
	// header
	req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
	req_content->header.__set_msg_name(NODE_POWEROFF_TASK_REQ);
	req_content->header.__set_nonce(util::create_nonce());
	req_content->header.__set_session_id(head_session_id);
	std::map<std::string, std::string> exten_info;
	exten_info["pub_key"] = body.pub_key;
	req_content->header.__set_exten_info(exten_info);
	std::vector<std::string> path;
	path.push_back(ConfManager::instance().GetNodeId());
	req_content->header.__set_path(path);

	// body
	dbc::node_poweroff_task_req_data req_data;
	req_data.__set_task_id(body.task_id);
	req_data.__set_peer_nodes_list(body.peer_nodes_list);
	req_data.__set_additional(body.additional);
	req_data.__set_wallet(body.wallet);
	req_data.__set_nonce(body.nonce);
	req_data.__set_sign(body.sign);
	req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
	req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
	std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
	for (auto& it : body.multisig_accounts.signs) {
		dbc::multisig_sign_item item;
		item.wallet = it.wallet;
		item.nonce = it.nonce;
		item.sign = it.sign;
		vecMultisigSignItem.push_back(item);
	}
	req_data.__set_multisig_signs(vecMultisigSignItem);
	req_data.__set_session_id(body.session_id);
	req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

	// encrypt
	std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
	network::binary_protocol proto(out_buf.get());
	req_data.write(&proto);

	auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
	if (service_info != nullptr) {
		std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
		std::string priv_key = ConfManager::instance().GetPrivKey();

		if (!pub_key.empty() && !priv_key.empty()) {
			std::string s_data = encrypt_data((unsigned char*)out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
			req_content->body.__set_data(s_data);
		} else {
			LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
			return nullptr;
		}
	} else {
		LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
		return nullptr;
	}

	std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
	req_msg->set_name(NODE_POWEROFF_TASK_REQ);
	req_msg->set_content(req_content);

	return req_msg;
}

void rest_api_service::on_node_poweroff_task_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
	const std::shared_ptr<network::message>& rsp_msg) {
	auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_poweroff_task_rsp>(rsp_msg->content);
	if (!node_rsp_msg) {
		LOG_ERROR << "node_rsp_msg is nullptr";
		return;
	}

	const std::shared_ptr<network::http_request>& httpReq = hreq_context->m_hreq;

	// decrypt
	std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
	std::string priv_key = ConfManager::instance().GetPrivKey();
	if (pub_key.empty() || priv_key.empty()) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
		LOG_ERROR << "pub_key or priv_key is empty";
		return;
	}

	std::string ori_message;
	try {
		bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
		if (!succ || ori_message.empty()) {
			httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
			LOG_ERROR << "rsq decrypt error1";
			return;
		}
	}
	catch (std::exception& e) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
		LOG_ERROR << "req decrypt error2";
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
	if (!ok) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
		LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
		return;
	}

	httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_poweroff_task_timer(const std::shared_ptr<core_timer>& timer) {
	if (nullptr == timer) {
		LOG_ERROR << "timer is nullptr";
		return;
	}

	const std::string& session_id = timer->get_session_id();
	std::shared_ptr<service_session> session = get_session(session_id);
	if (nullptr == session) {
		LOG_ERROR << "session is nullptr";
		return;
	}

	variables_map& vm = session->get_context().get_args();
	if (0 == vm.count(HTTP_REQUEST_KEY)) {
		LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
		session->clear();
		this->remove_session(session_id);
		return;
	}

	auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
	if (nullptr != hreq_context) {
		hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "stop task timeout");
	}

	session->clear();
	this->remove_session(session_id);
}

// stop task
void rest_api_service::rest_stop_task(const std::shared_ptr<network::http_request>& httpReq, const std::string& path) {
	if (httpReq->get_request_method() != network::http_request::POST) {
		LOG_ERROR << "http request is not post";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
		return;
	}

	std::vector<std::string> path_list;
	util::split_path(path, path_list);
	if (path_list.size() != 2) {
		LOG_ERROR << "path_list's size != 2";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /tasks/stop/<task_id>");
		return;
	}

	req_body body;

	body.task_id = path_list[1];
	if (body.task_id.empty()) {
		LOG_ERROR << "task_id is empty";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "task_id is empty");
		return;
	}

	std::string s_body = httpReq->read_body();
	if (s_body.empty()) {
		LOG_ERROR << "http request body is empty";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
	if (!ok) {
		std::stringstream ss;
		ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
		LOG_ERROR << ss.str();
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
		return;
	}

	if (!doc.IsObject()) {
		LOG_ERROR << "invalid json";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
		return;
	}

	std::string strerror;
	if (!parse_req_params(doc, body, strerror)) {
		LOG_ERROR << "parse req params failed: " << strerror;
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
		return;
	}

	if (!has_peer_nodeid(body)) {
		LOG_ERROR << "peer_nodeid is empty";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
		return;
	}

	if (body.session_id.empty() || body.session_id_sign.empty()) {
		if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
			LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
			return;
		}
	}

	// node request
	std::string head_session_id = util::create_session_id();

	auto node_req_msg = create_node_stop_task_req_msg(head_session_id, body);
	if (nullptr == node_req_msg) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
		return;
	}

	// timer
	uint32_t timer_id = add_timer(NODE_STOP_TASK_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
		CALLBACK_1(rest_api_service::on_node_stop_task_timer, this));
	if (INVALID_TIMER_ID == timer_id) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
		return;
	}

	// session
	if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
		remove_timer(timer_id);
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
		return;
	}

	// broadcast message
	if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
		return;
	}
}

std::shared_ptr<network::message> rest_api_service::create_node_stop_task_req_msg(const std::string& head_session_id,
	const req_body& body) {
	// 创建 node_ 请求
	std::shared_ptr<dbc::node_stop_task_req> req_content = std::make_shared<dbc::node_stop_task_req>();
	// header
	req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
	req_content->header.__set_msg_name(NODE_STOP_TASK_REQ);
	req_content->header.__set_nonce(util::create_nonce());
	req_content->header.__set_session_id(head_session_id);
	std::map<std::string, std::string> exten_info;
	exten_info["pub_key"] = body.pub_key;
	req_content->header.__set_exten_info(exten_info);
	std::vector<std::string> path;
	path.push_back(ConfManager::instance().GetNodeId());
	req_content->header.__set_path(path);

	// body
	dbc::node_stop_task_req_data req_data;
	req_data.__set_task_id(body.task_id);
	req_data.__set_peer_nodes_list(body.peer_nodes_list);
	req_data.__set_additional(body.additional);
	req_data.__set_wallet(body.wallet);
	req_data.__set_nonce(body.nonce);
	req_data.__set_sign(body.sign);
	req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
	req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
	std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
	for (auto& it : body.multisig_accounts.signs) {
		dbc::multisig_sign_item item;
		item.wallet = it.wallet;
		item.nonce = it.nonce;
		item.sign = it.sign;
		vecMultisigSignItem.push_back(item);
	}
	req_data.__set_multisig_signs(vecMultisigSignItem);
	req_data.__set_session_id(body.session_id);
	req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

	// encrypt
	std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
	network::binary_protocol proto(out_buf.get());
	req_data.write(&proto);

	auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
	if (service_info != nullptr) {
		std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
		std::string priv_key = ConfManager::instance().GetPrivKey();

		if (!pub_key.empty() && !priv_key.empty()) {
			std::string s_data = encrypt_data((unsigned char*)out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
			req_content->body.__set_data(s_data);
		}
		else {
			LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
			return nullptr;
		}
	}
	else {
		LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
		return nullptr;
	}

	std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
	req_msg->set_name(NODE_STOP_TASK_REQ);
	req_msg->set_content(req_content);

	return req_msg;
}

void rest_api_service::on_node_stop_task_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
	const std::shared_ptr<network::message>& rsp_msg) {
	auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_stop_task_rsp>(rsp_msg->content);
	if (!node_rsp_msg) {
		LOG_ERROR << "node_rsp_msg is nullptr";
		return;
	}

	const std::shared_ptr<network::http_request>& httpReq = hreq_context->m_hreq;

	// decrypt
	std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
	std::string priv_key = ConfManager::instance().GetPrivKey();
	if (pub_key.empty() || priv_key.empty()) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
		LOG_ERROR << "pub_key or priv_key is empty";
		return;
	}

	std::string ori_message;
	try {
		bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
		if (!succ || ori_message.empty()) {
			httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
			LOG_ERROR << "rsq decrypt error1";
			return;
		}
	}
	catch (std::exception& e) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
		LOG_ERROR << "req decrypt error2";
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
	if (!ok) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
		LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
		return;
	}

	httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_stop_task_timer(const std::shared_ptr<core_timer>& timer) {
	if (nullptr == timer) {
		LOG_ERROR << "timer is nullptr";
		return;
	}

	const std::string& session_id = timer->get_session_id();
	std::shared_ptr<service_session> session = get_session(session_id);
	if (nullptr == session) {
		LOG_ERROR << "session is nullptr";
		return;
	}

	variables_map& vm = session->get_context().get_args();
	if (0 == vm.count(HTTP_REQUEST_KEY)) {
		LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
		session->clear();
		this->remove_session(session_id);
		return;
	}

	auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
	if (nullptr != hreq_context) {
		hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "stop task timeout");
	}

	session->clear();
	this->remove_session(session_id);
}

// restart task
void rest_api_service::rest_restart_task(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 2) {
        LOG_ERROR << "path_list's size != 2";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /tasks/restart/<task_id>");
        return;
    }

    req_body body;

    std::map<std::string, std::string> query_table;
    util::split_path_into_kvs(path, query_table);

    std::string force_reboot = query_table.find("force_reboot") != query_table.end() ? query_table["force_reboot"] : "";
    int16_t enforce = 0;
    if (force_reboot == "true") {
        enforce = 1;
    } else if (!force_reboot.empty()) {
        enforce = atoi(force_reboot) == 0 ? 0 : 1;
    }
    body.force_reboot = enforce;

    body.task_id = path_list[1];
    if (body.task_id.empty()) {
        LOG_ERROR << "task_id is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "task_id is empty");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
            return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_restart_task_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_RESTART_TASK_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_restart_task_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_restart_task_req_msg(const std::string &head_session_id,
                    const req_body& body) {
    auto req_content = std::make_shared<dbc::node_restart_task_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_RESTART_TASK_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_restart_task_req_data req_data;
    req_data.__set_task_id(body.task_id);
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    if (body.force_reboot != 0) {
        req_data.__set_force_reboot(body.force_reboot);
    }
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_RESTART_TASK_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_restart_task_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
                                                   const std::shared_ptr<network::message>& rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_restart_task_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_restart_task_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "restart task timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// reset task
void rest_api_service::rest_reset_task(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 2) {
        LOG_ERROR << "path_list's size != 2";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /tasks/reset/<task_id>");
        return;
    }

    req_body body;

    body.task_id = path_list[1];
    if (body.task_id.empty()) {
        LOG_ERROR << "task_id is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "task_id is empty");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
            return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_reset_task_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_RESET_TASK_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_reset_task_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_reset_task_req_msg(const std::string &head_session_id,
                                                                                   const req_body& body) {
    std::shared_ptr<dbc::node_reset_task_req> req_content = std::make_shared<dbc::node_reset_task_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_RESET_TASK_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_reset_task_req_data req_data;
    req_data.__set_task_id(body.task_id);
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_RESET_TASK_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_reset_task_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
                                                 const std::shared_ptr<network::message>& rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_reset_task_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_reset_task_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "reset task timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// delete task
void rest_api_service::rest_delete_task(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 2) {
        LOG_ERROR << "path_list's size != 2";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /tasks/delete/<task_id>");
        return;
    }

    req_body body;

    body.task_id = path_list[1];
    if (body.task_id.empty()) {
        LOG_ERROR << "task_id is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "task_id is empty");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
            return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_delete_task_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_DELETE_TASK_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_delete_task_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_delete_task_req_msg(const std::string &head_session_id,
                                                                                    const req_body& body) {
    std::shared_ptr<dbc::node_delete_task_req> req_content = std::make_shared<dbc::node_delete_task_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_DELETE_TASK_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_delete_task_req_data req_data;
    req_data.__set_task_id(body.task_id);
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_DELETE_TASK_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_delete_task_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
                                                  const std::shared_ptr<network::message>& rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_delete_task_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "rsp is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_delete_task_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "delete task timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// modify task
void rest_api_service::rest_modify_task(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 2) {
        LOG_ERROR << "path_list's size != 2";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /tasks/modify/<task_id>");
        return;
    }

    req_body body;

    body.task_id = path_list[1];
    if (body.task_id.empty()) {
        LOG_ERROR << "task_id is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "task_id is empty");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
            return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_modify_task_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_MODIFY_TASK_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_modify_task_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_modify_task_req_msg(const std::string &head_session_id,
                                                                                         const req_body& body) {
    // 创建 node_ 请求
    std::shared_ptr<dbc::node_modify_task_req> req_content = std::make_shared<dbc::node_modify_task_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_MODIFY_TASK_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_modify_task_req_data req_data;
    req_data.__set_task_id(body.task_id);
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_MODIFY_TASK_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_modify_task_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
                                                  const std::shared_ptr<network::message>& rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_modify_task_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_modify_task_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "modify task timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// set task user password
void rest_api_service::rest_passwd_task(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 2) {
        LOG_ERROR << "path_list's size != 2";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /tasks/passwd/<task_id>");
        return;
    }

    req_body body;

    body.task_id = path_list[1];
    if (body.task_id.empty()) {
        LOG_ERROR << "task_id is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "task_id is empty");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
            return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_passwd_task_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_PASSWD_TASK_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_passwd_task_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_passwd_task_req_msg(const std::string &head_session_id,
                                                                                         const req_body& body) {
    // 创建 node_ 请求
    std::shared_ptr<dbc::node_passwd_task_req> req_content = std::make_shared<dbc::node_passwd_task_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_PASSWD_TASK_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_passwd_task_req_data req_data;
    req_data.__set_task_id(body.task_id);
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_PASSWD_TASK_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_passwd_task_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
                                                  const std::shared_ptr<network::message>& rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_passwd_task_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_passwd_task_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "set task user password timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// task logs
void rest_api_service::rest_task_logs(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 2) {
        LOG_ERROR << "path_list's size != 2";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /tasks/logs/{task_id}?flag=tail&line_num=100");
        return;
    }

    req_body body;

    std::map<std::string, std::string> query_table;
    util::split_path_into_kvs(path, query_table);

    std::string head_or_tail = query_table.find("flag") != query_table.end() ? query_table["flag"] : "";
    std::string number_of_lines = query_table.find("line_num") != query_table.end() ? query_table["line_num"] : "";

    if (head_or_tail.empty()) {
        head_or_tail = "tail";
        number_of_lines = "100";
    } else if (head_or_tail == "head" || head_or_tail == "tail") {
        if (number_of_lines.empty()) {
            number_of_lines = "100";
        }
    } else {
        LOG_ERROR << "head_or_tail is invalid";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "head_or_tail is invalid\"");
        return;
    }

    // number of lines
    int32_t lines = atoi(number_of_lines);
    if (lines > MAX_NUMBER_OF_LINES) {
        lines = MAX_NUMBER_OF_LINES - 1;
    } else if (lines <= 0) {
        LOG_ERROR << "invalid line num";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid line num");
        return;
    }
    body.number_of_lines = lines;

    // tail or head
    if (head_or_tail == "tail") {
        body.head_or_tail = QUERY_LOG_DIRECTION::Tail;
    } else if (head_or_tail == "head") {
        body.head_or_tail = QUERY_LOG_DIRECTION::Head;
    } else {
        body.head_or_tail = QUERY_LOG_DIRECTION::Tail;
    }

    body.task_id = path_list[1];
    if (body.task_id.empty()) {
        LOG_ERROR << "task_id is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "task_id is empty");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
            return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_task_logs_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_TASK_LOGS_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_task_logs_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_task_logs_req_msg(const std::string &head_session_id,
                                                   const req_body& body) {
    auto req_content = std::make_shared<dbc::node_task_logs_req>();
    //header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_TASK_LOGS_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    //body
    dbc::node_task_logs_req_data req_data;
    req_data.__set_task_id(body.task_id);
    req_data.__set_head_or_tail((int16_t) body.head_or_tail);
    req_data.__set_number_of_lines(body.number_of_lines);
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_TASK_LOGS_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_task_logs_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
                                                const std::shared_ptr<network::message>& rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_task_logs_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_task_logs_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "task logs timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// images
void
rest_api_service::rest_images(const std::shared_ptr<network::http_request>& httpReq, const std::string& path) {
	std::vector<std::string> path_list;
	util::split_path(path, path_list);

	if (path_list.empty()) {
		rest_list_images(httpReq, path);
		return;
	}

	if (path_list.size() == 1) {
		const std::string& second_param = path_list[0];
		if (second_param == "servers") {
			rest_list_image_servers(httpReq, path);
			return;
		} else if (second_param == "download") {
			rest_download_image(httpReq, path);
			return;
		} else if (second_param == "downloadprogress") {
			rest_download_image_progress(httpReq, path);
			return;
		} else if (second_param == "download_stop") {
			rest_stop_download_image(httpReq, path);
			return;
		} else if (second_param == "upload") {
			rest_upload_image(httpReq, path);
			return;
		} else if (second_param == "uploadprogress") {
			rest_upload_image_progress(httpReq, path);
			return;
		} else if (second_param == "upload_stop") {
			rest_stop_upload_image(httpReq, path);
			return;
		} else if (second_param == "delete") {
			rest_delete_image(httpReq, path);
			return;
		}
	}

	httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "invalid requests uri");
}

// list image servers
void rest_api_service::rest_list_image_servers(const std::shared_ptr<network::http_request>& httpReq,
	const std::string& path) {
	if (httpReq->get_request_method() != network::http_request::POST) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
		return;
	}

	req_body body;

	// parse body
	std::string s_body = httpReq->read_body();
	if (s_body.empty()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
	if (!ok) {
		std::stringstream ss;
		ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
		return;
	}

	if (!doc.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
		return;
	}

	// image_servers
	std::stringstream ss;
	ss << "{";
	ss << "\"errcode\":0";
	ss << ", \"message\":{";
	ss << "\"image_server\":[";
	auto& servers = ConfManager::instance().GetImageServers();
	int count = 0;
	for (auto& it : servers) {
		if (count > 0) ss << ",";
		ss << "\"" << it.second->id << "\"";
		++count;
	}
	ss << "]}";
	ss << "}";

	httpReq->reply_comm_rest_succ2(ss.str());
}


// list images
void rest_api_service::rest_list_images(const std::shared_ptr<network::http_request>& httpReq,
	const std::string& path) {
	if (httpReq->get_request_method() != network::http_request::POST) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
		return;
	}

	req_body body;

	// parse body
	std::string s_body = httpReq->read_body();
	if (s_body.empty()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
	if (!ok) {
		std::stringstream ss;
		ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code());
		LOG_ERROR << ss.str();
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
		return;
	}

	if (!doc.IsObject()) {
		LOG_ERROR << "invalid json";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
		return;
	}

	std::string strerror;
	if (!parse_req_params(doc, body, strerror)) {
		LOG_ERROR << "parse request params failed: " << strerror;
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}

	const rapidjson::Value& v_additional = doc["additional"];
	if (!v_additional.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}
	// image_server
	std::string image_server;
	JSON_PARSE_STRING(v_additional, "image_server", image_server)

		// list images from image_server
		if (!has_peer_nodeid(body)) {
			auto it_svr = ConfManager::instance().FindImageServer(image_server);
			if (image_server.empty() || it_svr == nullptr) {
				httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "image_server not exist");
			} else {
				std::vector<ImageFile> v_images;
				ImageManager::instance().listShareImages(*it_svr, v_images);

				std::string rsp_json = "{";
				rsp_json += "\"errcode\":0";
				rsp_json += ",\"message\": [";
				int count = 0;
				for (int i = 0; i < v_images.size(); i++) {
					if (count > 0) rsp_json += ",";

					rsp_json += "{";
					rsp_json += "\"image_name\":\"" + v_images[i].name + "\"";
					rsp_json += ",\"image_size\":\"" + scale_size(v_images[i].size / 1024L) + "\"";
					rsp_json += "}";

					count += 1;
				}
				rsp_json += "]";
				rsp_json += "}";

				httpReq->reply_comm_rest_succ2(rsp_json);
			}
		}
	// list images from compute_node
		else {
			auto it_svr = ConfManager::instance().FindImageServer(image_server);
			if (!image_server.empty() && it_svr == nullptr) {
				httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "image_server not exist");
				return;
			}
			if (it_svr != nullptr)
				body.image_server = it_svr->to_string();

			// node request
			std::string head_session_id = util::create_session_id();

			auto node_req_msg = create_node_list_images_req_msg(head_session_id, body);
			if (nullptr == node_req_msg) {
				httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
				return;
			}

			// timer
			uint32_t timer_id = add_timer(NODE_LIST_IMAGES_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
				CALLBACK_1(rest_api_service::on_node_list_images_timer, this));
			if (INVALID_TIMER_ID == timer_id) {
				httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
				return;
			}

			// session
			if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id,
				body.peer_nodes_list[0])) {
				remove_timer(timer_id);
				httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
				return;
			}

			// broadcast message
			if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
				httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
				return;
			}
		}
}

std::shared_ptr<network::message>
rest_api_service::create_node_list_images_req_msg(const std::string& head_session_id, const req_body& body) {
	auto req_content = std::make_shared<dbc::node_list_images_req>();
	// header
	req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
	req_content->header.__set_msg_name(NODE_LIST_IMAGES_REQ);
	req_content->header.__set_nonce(util::create_nonce());
	req_content->header.__set_session_id(head_session_id);
	std::map<std::string, std::string> exten_info;
	exten_info["pub_key"] = body.pub_key;
	req_content->header.__set_exten_info(exten_info);
	std::vector<std::string> path;
	path.push_back(ConfManager::instance().GetNodeId());
	req_content->header.__set_path(path);
	// body
	dbc::node_list_images_req_data req_data;
	req_data.__set_peer_nodes_list(body.peer_nodes_list);
	req_data.__set_additional(body.additional);
	req_data.__set_wallet(body.wallet);
	req_data.__set_nonce(body.nonce);
	req_data.__set_sign(body.sign);
	req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
	req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
	std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
	for (auto& it : body.multisig_accounts.signs) {
		dbc::multisig_sign_item item;
		item.wallet = it.wallet;
		item.nonce = it.nonce;
		item.sign = it.sign;
		vecMultisigSignItem.push_back(item);
	}
	req_data.__set_multisig_signs(vecMultisigSignItem);
	req_data.__set_session_id(body.session_id);
	req_data.__set_session_id_sign(body.session_id_sign);
	req_data.__set_image_server(body.image_server);
    req_data.__set_rent_order(body.rent_order);

	// encrypt
	std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
	network::binary_protocol proto(out_buf.get());
	req_data.write(&proto);

	auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
	if (service_info != nullptr) {
		std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
		std::string priv_key = ConfManager::instance().GetPrivKey();

		if (!pub_key.empty() && !priv_key.empty()) {
			std::string s_data = encrypt_data((unsigned char*)out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
			req_content->body.__set_data(s_data);
		} else {
			LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
			return nullptr;
		}
	} else {
		LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
		return nullptr;
	}

	std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
	req_msg->set_name(NODE_LIST_IMAGES_REQ);
	req_msg->set_content(req_content);

	return req_msg;
}

void rest_api_service::on_node_list_images_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
	const std::shared_ptr<network::message>& rsp_msg) {
	auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_list_images_rsp>(rsp_msg->content);
	if (!node_rsp_msg) {
		LOG_ERROR << "node rsp msg (list images) is nullptr";
		return;
	}

	const std::shared_ptr<network::http_request>& httpReq = hreq_context->m_hreq;

	// decrypt
	std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
	std::string priv_key = ConfManager::instance().GetPrivKey();
	if (pub_key.empty() || priv_key.empty()) {
		LOG_ERROR << "pub_key or priv_key is empty";
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
		return;
	}

	std::string ori_message;
	try {
		bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
		if (!succ || ori_message.empty()) {
			LOG_ERROR << "rsp decrypt error1";
			httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error1");
			return;
		}
	}
	catch (std::exception& e) {
		LOG_ERROR << "rsp decrypt error2";
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error2");
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
	if (!ok) {
		LOG_ERROR << "http response parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "http response parse error");
		return;
	}

	httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_list_images_timer(const std::shared_ptr<core_timer>& timer) {
	if (nullptr == timer) {
		LOG_ERROR << "timer is nullptr";
		return;
	}

	const std::string& session_id = timer->get_session_id();
	std::shared_ptr<service_session> session = get_session(session_id);
	if (nullptr == session) {
		LOG_ERROR << "session is nullptr";
		return;
	}

	variables_map& vm = session->get_context().get_args();
	if (0 == vm.count(HTTP_REQUEST_KEY)) {
		LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
		session->clear();
		this->remove_session(session_id);
		return;
	}

	auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
	if (nullptr != hreq_context) {
		LOG_ERROR << "list images timout";
		hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "list images timeout");
	}

	session->clear();
	this->remove_session(session_id);
}

// download image
void rest_api_service::rest_download_image(const std::shared_ptr<network::http_request>& httpReq,
	const std::string& path) {
	if (httpReq->get_request_method() != network::http_request::POST) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
		return;
	}

	req_body body;

	// parse body
	std::string s_body = httpReq->read_body();
	if (s_body.empty()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
	if (!ok) {
		std::stringstream ss;
		ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
		return;
	}

	if (!doc.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
		return;
	}

	std::string strerror;
	if (!parse_req_params(doc, body, strerror)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}

	if (!has_peer_nodeid(body)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
		return;
	}

	// image_server
	const rapidjson::Value& v_additional = doc["additional"];
	if (!v_additional.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}
	std::string image_server;
	JSON_PARSE_STRING(v_additional, "image_server", image_server)

		auto it_svr = ConfManager::instance().FindImageServer(image_server);
	if (image_server.empty() || it_svr == nullptr) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "image_server not exist");
		return;
	}
	body.image_server = it_svr->to_string();

	// session_id wallet
	if (body.session_id.empty() || body.session_id_sign.empty()) {
		if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "session_id and wallet all invalid");
			return;
		}
	}

	// node request
	std::string head_session_id = util::create_session_id();

	auto node_req_msg = create_node_download_image_req_msg(head_session_id, body);
	if (nullptr == node_req_msg) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
		return;
	}

	// timer
	uint32_t timer_id = add_timer(NODE_DOWNLOAD_IMAGE_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
		CALLBACK_1(rest_api_service::on_node_download_image_timer, this));
	if (INVALID_TIMER_ID == timer_id) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
		return;
	}

	// session
	if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
		remove_timer(timer_id);
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
		return;
	}

	// broadcast message
	if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
		return;
	}
}

std::shared_ptr<network::message>
rest_api_service::create_node_download_image_req_msg(const std::string& head_session_id, const req_body& body) {
	// 创建 node_ 请求
	auto req_content = std::make_shared<dbc::node_download_image_req>();
	// header
	req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
	req_content->header.__set_msg_name(NODE_DOWNLOAD_IMAGE_REQ);
	req_content->header.__set_nonce(util::create_nonce());
	req_content->header.__set_session_id(head_session_id);
	std::map<std::string, std::string> exten_info;
	exten_info["pub_key"] = body.pub_key;
	req_content->header.__set_exten_info(exten_info);
	std::vector<std::string> path;
	path.push_back(ConfManager::instance().GetNodeId());
	req_content->header.__set_path(path);
	// body
	dbc::node_download_image_req_data req_data;
	req_data.__set_peer_nodes_list(body.peer_nodes_list);
	req_data.__set_additional(body.additional);
	req_data.__set_wallet(body.wallet);
	req_data.__set_nonce(body.nonce);
	req_data.__set_sign(body.sign);
	req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
	req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
	std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
	for (auto& it : body.multisig_accounts.signs) {
		dbc::multisig_sign_item item;
		item.wallet = it.wallet;
		item.nonce = it.nonce;
		item.sign = it.sign;
		vecMultisigSignItem.push_back(item);
	}
	req_data.__set_multisig_signs(vecMultisigSignItem);
	req_data.__set_session_id(body.session_id);
	req_data.__set_session_id_sign(body.session_id_sign);
	req_data.__set_image_server(body.image_server);
    req_data.__set_rent_order(body.rent_order);

	// encrypt
	std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
	network::binary_protocol proto(out_buf.get());
	req_data.write(&proto);

	auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
	if (service_info != nullptr) {
		std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
		std::string priv_key = ConfManager::instance().GetPrivKey();

		if (!pub_key.empty() && !priv_key.empty()) {
			std::string s_data = encrypt_data((unsigned char*)out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
			req_content->body.__set_data(s_data);
		} else {
			LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
			return nullptr;
		}
	} else {
		LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
		return nullptr;
	}

	std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
	req_msg->set_name(NODE_DOWNLOAD_IMAGE_REQ);
	req_msg->set_content(req_content);

	return req_msg;
}

void
rest_api_service::on_node_download_image_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
	const std::shared_ptr<network::message>& rsp_msg) {
	auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_download_image_rsp>(rsp_msg->content);
	if (!node_rsp_msg) {
		LOG_ERROR << "node_rsp_msg is nullptr";
		return;
	}

	const std::shared_ptr<network::http_request>& httpReq = hreq_context->m_hreq;

	// decrypt
	std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
	std::string priv_key = ConfManager::instance().GetPrivKey();
	if (pub_key.empty() || priv_key.empty()) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
		LOG_ERROR << "pub_key or priv_key is empty";
		return;
	}

	std::string ori_message;
	try {
		bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
		if (!succ || ori_message.empty()) {
			httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error1");
			LOG_ERROR << "rsp decrypt error1";
			return;
		}
	}
	catch (std::exception& e) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error2");
		LOG_ERROR << "rsp decrypt error2";
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
	if (!ok) {
		LOG_ERROR << "http response parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "http response parse error");
		return;
	}

	httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_download_image_timer(const std::shared_ptr<core_timer>& timer) {
	if (nullptr == timer) {
		LOG_ERROR << "timer is nullptr";
		return;
	}

	const std::string& session_id = timer->get_session_id();
	std::shared_ptr<service_session> session = get_session(session_id);
	if (nullptr == session) {
		LOG_ERROR << "session is nullptr";
		return;
	}

	variables_map& vm = session->get_context().get_args();
	if (0 == vm.count(HTTP_REQUEST_KEY)) {
		LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
		session->clear();
		this->remove_session(session_id);
		return;
	}

	auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
	if (nullptr != hreq_context) {
		LOG_ERROR << "download image timeout";
		hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "download image timeout");
	}

	session->clear();
	this->remove_session(session_id);
}

// download progress
void rest_api_service::rest_download_image_progress(const std::shared_ptr<network::http_request>& httpReq,
	const std::string& path) {
	if (httpReq->get_request_method() != network::http_request::POST) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
		return;
	}

	req_body body;

	// parse body
	std::string s_body = httpReq->read_body();
	if (s_body.empty()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
	if (!ok) {
		std::stringstream ss;
		ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
		return;
	}

	if (!doc.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
		return;
	}

	std::string strerror;
	if (!parse_req_params(doc, body, strerror)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}

	if (!has_peer_nodeid(body)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
		return;
	}

	// session_id wallet
	if (body.session_id.empty() || body.session_id_sign.empty()) {
		if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "session_id and wallet all invalid");
			return;
		}
	}

	// node request
	std::string head_session_id = util::create_session_id();

	auto node_req_msg = create_node_download_image_progress_req_msg(head_session_id, body);
	if (nullptr == node_req_msg) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
		return;
	}

	// timer
	uint32_t timer_id = add_timer(NODE_DOWNLOAD_IMAGE_PROGRESS_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
		CALLBACK_1(rest_api_service::on_node_download_image_progress_timer, this));
	if (INVALID_TIMER_ID == timer_id) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
		return;
	}

	// session
	if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
		remove_timer(timer_id);
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
		return;
	}

	// broadcast message
	if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
		return;
	}
}

std::shared_ptr<network::message>
rest_api_service::create_node_download_image_progress_req_msg(const std::string& head_session_id, const req_body& body) {
	// 创建 node_ 请求
	auto req_content = std::make_shared<dbc::node_download_image_progress_req>();
	// header
	req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
	req_content->header.__set_msg_name(NODE_DOWNLOAD_IMAGE_PROGRESS_REQ);
	req_content->header.__set_nonce(util::create_nonce());
	req_content->header.__set_session_id(head_session_id);
	std::map<std::string, std::string> exten_info;
	exten_info["pub_key"] = body.pub_key;
	req_content->header.__set_exten_info(exten_info);
	std::vector<std::string> path;
	path.push_back(ConfManager::instance().GetNodeId());
	req_content->header.__set_path(path);
	// body
	dbc::node_download_image_progress_req_data req_data;
	req_data.__set_peer_nodes_list(body.peer_nodes_list);
	req_data.__set_additional(body.additional);
	req_data.__set_wallet(body.wallet);
	req_data.__set_nonce(body.nonce);
	req_data.__set_sign(body.sign);
	req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
	req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
	std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
	for (auto& it : body.multisig_accounts.signs) {
		dbc::multisig_sign_item item;
		item.wallet = it.wallet;
		item.nonce = it.nonce;
		item.sign = it.sign;
		vecMultisigSignItem.push_back(item);
	}
	req_data.__set_multisig_signs(vecMultisigSignItem);
	req_data.__set_session_id(body.session_id);
	req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

	// encrypt
	std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
	network::binary_protocol proto(out_buf.get());
	req_data.write(&proto);

	auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
	if (service_info != nullptr) {
		std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
		std::string priv_key = ConfManager::instance().GetPrivKey();

		if (!pub_key.empty() && !priv_key.empty()) {
			std::string s_data = encrypt_data((unsigned char*)out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
			req_content->body.__set_data(s_data);
		} else {
			LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
			return nullptr;
		}
	} else {
		LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
		return nullptr;
	}

	std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
	req_msg->set_name(NODE_DOWNLOAD_IMAGE_PROGRESS_REQ);
	req_msg->set_content(req_content);

	return req_msg;
}

void
rest_api_service::on_node_download_image_progress_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
	const std::shared_ptr<network::message>& rsp_msg) {
	auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_download_image_progress_rsp>(rsp_msg->content);
	if (!node_rsp_msg) {
		LOG_ERROR << "node_rsp_msg is nullptr";
		return;
	}

	const std::shared_ptr<network::http_request>& httpReq = hreq_context->m_hreq;

	// decrypt
	std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
	std::string priv_key = ConfManager::instance().GetPrivKey();
	if (pub_key.empty() || priv_key.empty()) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
		LOG_ERROR << "pub_key or priv_key is empty";
		return;
	}

	std::string ori_message;
	try {
		bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
		if (!succ || ori_message.empty()) {
			httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error1");
			LOG_ERROR << "rsp decrypt error1";
			return;
		}
	}
	catch (std::exception& e) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error2");
		LOG_ERROR << "rsp decrypt error2";
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
	if (!ok) {
		LOG_ERROR << "http response parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "http response parse error");
		return;
	}

	httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_download_image_progress_timer(const std::shared_ptr<core_timer>& timer) {
	if (nullptr == timer) {
		LOG_ERROR << "timer is nullptr";
		return;
	}

	const std::string& session_id = timer->get_session_id();
	std::shared_ptr<service_session> session = get_session(session_id);
	if (nullptr == session) {
		LOG_ERROR << "session is nullptr";
		return;
	}

	variables_map& vm = session->get_context().get_args();
	if (0 == vm.count(HTTP_REQUEST_KEY)) {
		LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
		session->clear();
		this->remove_session(session_id);
		return;
	}

	auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
	if (nullptr != hreq_context) {
		LOG_ERROR << "query download image progress timeout";
		hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "query download image progress timeout");
	}

	session->clear();
	this->remove_session(session_id);
}

// stop download
void rest_api_service::rest_stop_download_image(const std::shared_ptr<network::http_request>& httpReq,
	const std::string& path) {
	if (httpReq->get_request_method() != network::http_request::POST) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
		return;
	}

	req_body body;

	// parse body
	std::string s_body = httpReq->read_body();
	if (s_body.empty()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
	if (!ok) {
		std::stringstream ss;
		ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
		return;
	}

	if (!doc.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
		return;
	}

	std::string strerror;
	if (!parse_req_params(doc, body, strerror)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}

	if (!has_peer_nodeid(body)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
		return;
	}

	// session_id wallet
	if (body.session_id.empty() || body.session_id_sign.empty()) {
		if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "session_id and wallet all invalid");
			return;
		}
	}

	// node request
	std::string head_session_id = util::create_session_id();

	auto node_req_msg = create_node_stop_download_image_req_msg(head_session_id, body);
	if (nullptr == node_req_msg) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
		return;
	}

	// timer
	uint32_t timer_id = add_timer(NODE_STOP_DOWNLOAD_IMAGE_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
		CALLBACK_1(rest_api_service::on_node_stop_download_image_timer, this));
	if (INVALID_TIMER_ID == timer_id) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
		return;
	}

	// session
	if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
		remove_timer(timer_id);
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
		return;
	}

	// broadcast message
	if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
		return;
	}
}

std::shared_ptr<network::message>
rest_api_service::create_node_stop_download_image_req_msg(const std::string& head_session_id, const req_body& body) {
	// 创建 node_ 请求
	auto req_content = std::make_shared<dbc::node_stop_download_image_req>();
	// header
	req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
	req_content->header.__set_msg_name(NODE_STOP_DOWNLOAD_IMAGE_REQ);
	req_content->header.__set_nonce(util::create_nonce());
	req_content->header.__set_session_id(head_session_id);
	std::map<std::string, std::string> exten_info;
	exten_info["pub_key"] = body.pub_key;
	req_content->header.__set_exten_info(exten_info);
	std::vector<std::string> path;
	path.push_back(ConfManager::instance().GetNodeId());
	req_content->header.__set_path(path);
	// body
	dbc::node_stop_download_image_req_data req_data;
	req_data.__set_peer_nodes_list(body.peer_nodes_list);
	req_data.__set_additional(body.additional);
	req_data.__set_wallet(body.wallet);
	req_data.__set_nonce(body.nonce);
	req_data.__set_sign(body.sign);
	req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
	req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
	std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
	for (auto& it : body.multisig_accounts.signs) {
		dbc::multisig_sign_item item;
		item.wallet = it.wallet;
		item.nonce = it.nonce;
		item.sign = it.sign;
		vecMultisigSignItem.push_back(item);
	}
	req_data.__set_multisig_signs(vecMultisigSignItem);
	req_data.__set_session_id(body.session_id);
	req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

	// encrypt
	std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
	network::binary_protocol proto(out_buf.get());
	req_data.write(&proto);

	auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
	if (service_info != nullptr) {
		std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
		std::string priv_key = ConfManager::instance().GetPrivKey();

		if (!pub_key.empty() && !priv_key.empty()) {
			std::string s_data = encrypt_data((unsigned char*)out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
			req_content->body.__set_data(s_data);
		} else {
			LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
			return nullptr;
		}
	} else {
		LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
		return nullptr;
	}

	std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
	req_msg->set_name(NODE_STOP_DOWNLOAD_IMAGE_REQ);
	req_msg->set_content(req_content);

	return req_msg;
}

void
rest_api_service::on_node_stop_download_image_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
	const std::shared_ptr<network::message>& rsp_msg) {
	auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_stop_download_image_rsp>(rsp_msg->content);
	if (!node_rsp_msg) {
		LOG_ERROR << "node_rsp_msg is nullptr";
		return;
	}

	const std::shared_ptr<network::http_request>& httpReq = hreq_context->m_hreq;

	// decrypt
	std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
	std::string priv_key = ConfManager::instance().GetPrivKey();
	if (pub_key.empty() || priv_key.empty()) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
		LOG_ERROR << "pub_key or priv_key is empty";
		return;
	}

	std::string ori_message;
	try {
		bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
		if (!succ || ori_message.empty()) {
			httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error1");
			LOG_ERROR << "rsp decrypt error1";
			return;
		}
	}
	catch (std::exception& e) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error2");
		LOG_ERROR << "rsp decrypt error2";
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
	if (!ok) {
		LOG_ERROR << "http response parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "http response parse error");
		return;
	}

	httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_stop_download_image_timer(const std::shared_ptr<core_timer>& timer) {
	if (nullptr == timer) {
		LOG_ERROR << "timer is nullptr";
		return;
	}

	const std::string& session_id = timer->get_session_id();
	std::shared_ptr<service_session> session = get_session(session_id);
	if (nullptr == session) {
		LOG_ERROR << "session is nullptr";
		return;
	}

	variables_map& vm = session->get_context().get_args();
	if (0 == vm.count(HTTP_REQUEST_KEY)) {
		LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
		session->clear();
		this->remove_session(session_id);
		return;
	}

	auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
	if (nullptr != hreq_context) {
		LOG_ERROR << "stop download image timeout";
		hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "download image timeout");
	}

	session->clear();
	this->remove_session(session_id);
}

// upload image
void rest_api_service::rest_upload_image(const std::shared_ptr<network::http_request>& httpReq,
	const std::string& path) {
	if (httpReq->get_request_method() != network::http_request::POST) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
		return;
	}

	req_body body;

	// parse body
	std::string s_body = httpReq->read_body();
	if (s_body.empty()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
	if (!ok) {
		std::stringstream ss;
		ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
		return;
	}

	if (!doc.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
		return;
	}

	std::string strerror;
	if (!parse_req_params(doc, body, strerror)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}

	const rapidjson::Value& v_additional = doc["additional"];
	if (!v_additional.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}

	// image_filename
	std::string image_filename;
	JSON_PARSE_STRING(v_additional, "image_filename", image_filename);

	// image_server
	std::string image_server;
	JSON_PARSE_STRING(v_additional, "image_server", image_server)

		auto it_svr = ConfManager::instance().FindImageServer(image_server);
	if (image_server.empty() || it_svr == nullptr) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "image_server not exist");
		return;
	}

	// 从client节点上传镜像到镜像中心(默认路径: /data/)
	if (!has_peer_nodeid(body)) {
		if (!boost::filesystem::path(image_filename).is_absolute())
			image_filename = "/data/" + image_filename;

		if (image_filename.empty() || !boost::filesystem::exists(image_filename)) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "image_filename: /data/" + image_filename + " not exist");
			return;
		}

		ImageServer imgsvr;
		imgsvr.from_string(image_server);
		ImageManager::instance().upload(image_filename, imgsvr);

		std::string rsp_json = "{";
		rsp_json += "\"errcode\":0";
		rsp_json += ",\"message\": \"ok\"";
		rsp_json += "}";

		httpReq->reply_comm_rest_succ2(rsp_json);
	}
	// 从node_id指定的算力节点上传镜像到镜像中心
	else {
		body.image_server = it_svr->to_string();

		// session_id wallet
		if (body.session_id.empty() || body.session_id_sign.empty()) {
			if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
				httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "session_id and wallet all invalid");
				return;
			}
		}

		// node request
		std::string head_session_id = util::create_session_id();

		auto node_req_msg = create_node_upload_image_req_msg(head_session_id, body);
		if (nullptr == node_req_msg) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
			return;
		}

		// timer
		uint32_t timer_id = add_timer(NODE_UPLOAD_IMAGE_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
			CALLBACK_1(rest_api_service::on_node_upload_image_timer, this));
		if (INVALID_TIMER_ID == timer_id) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
			return;
		}

		// session
		if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id,
			body.peer_nodes_list[0])) {
			remove_timer(timer_id);
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
			return;
		}

		// broadcast message
		if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
			return;
		}
	}
}

std::shared_ptr<network::message>
rest_api_service::create_node_upload_image_req_msg(const std::string& head_session_id, const req_body& body) {
	// 创建 node_ 请求
	auto req_content = std::make_shared<dbc::node_upload_image_req>();
	// header
	req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
	req_content->header.__set_msg_name(NODE_UPLOAD_IMAGE_REQ);
	req_content->header.__set_nonce(util::create_nonce());
	req_content->header.__set_session_id(head_session_id);
	std::map<std::string, std::string> exten_info;
	exten_info["pub_key"] = body.pub_key;
	req_content->header.__set_exten_info(exten_info);
	std::vector<std::string> path;
	path.push_back(ConfManager::instance().GetNodeId());
	req_content->header.__set_path(path);
	// body
	dbc::node_upload_image_req_data req_data;
	req_data.__set_peer_nodes_list(body.peer_nodes_list);
	req_data.__set_additional(body.additional);
	req_data.__set_wallet(body.wallet);
	req_data.__set_nonce(body.nonce);
	req_data.__set_sign(body.sign);
	req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
	req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
	std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
	for (auto& it : body.multisig_accounts.signs) {
		dbc::multisig_sign_item item;
		item.wallet = it.wallet;
		item.nonce = it.nonce;
		item.sign = it.sign;
		vecMultisigSignItem.push_back(item);
	}
	req_data.__set_multisig_signs(vecMultisigSignItem);
	req_data.__set_session_id(body.session_id);
	req_data.__set_session_id_sign(body.session_id_sign);
	req_data.__set_image_server(body.image_server);
    req_data.__set_rent_order(body.rent_order);

	// encrypt
	std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
	network::binary_protocol proto(out_buf.get());
	req_data.write(&proto);

	auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
	if (service_info != nullptr) {
		std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
		std::string priv_key = ConfManager::instance().GetPrivKey();

		if (!pub_key.empty() && !priv_key.empty()) {
			std::string s_data = encrypt_data((unsigned char*)out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
			req_content->body.__set_data(s_data);
		} else {
			LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
			return nullptr;
		}
	} else {
		LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
		return nullptr;
	}

	std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
	req_msg->set_name(NODE_UPLOAD_IMAGE_REQ);
	req_msg->set_content(req_content);

	return req_msg;
}

void rest_api_service::on_node_upload_image_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
	const std::shared_ptr<network::message>& rsp_msg) {
	auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_upload_image_rsp>(rsp_msg->content);
	if (!node_rsp_msg) {
		LOG_ERROR << "node_rsp_msg is nullptr";
		return;
	}

	const std::shared_ptr<network::http_request>& httpReq = hreq_context->m_hreq;

	// decrypt
	std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
	std::string priv_key = ConfManager::instance().GetPrivKey();
	if (pub_key.empty() || priv_key.empty()) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
		LOG_ERROR << "pub_key or priv_key is empty";
		return;
	}

	std::string ori_message;
	try {
		bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
		if (!succ || ori_message.empty()) {
			httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error1");
			LOG_ERROR << "rsp decrypt error1";
			return;
		}
	}
	catch (std::exception& e) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error2");
		LOG_ERROR << "rsp decrypt error2";
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
	if (!ok) {
		LOG_ERROR << "http response parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "http response parse error");
		return;
	}

	httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_upload_image_timer(const std::shared_ptr<core_timer>& timer) {
	if (nullptr == timer) {
		LOG_ERROR << "timer is nullptr";
		return;
	}

	const std::string& session_id = timer->get_session_id();
	std::shared_ptr<service_session> session = get_session(session_id);
	if (nullptr == session) {
		LOG_ERROR << "session is nullptr";
		return;
	}

	variables_map& vm = session->get_context().get_args();
	if (0 == vm.count(HTTP_REQUEST_KEY)) {
		LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
		session->clear();
		this->remove_session(session_id);
		return;
	}

	auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
	if (nullptr != hreq_context) {
		LOG_ERROR << "upload image timeout";
		hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "upload image timeout");
	}

	session->clear();
	this->remove_session(session_id);
}

// upload image progress
void rest_api_service::rest_upload_image_progress(const std::shared_ptr<network::http_request>& httpReq,
	const std::string& path) {
	if (httpReq->get_request_method() != network::http_request::POST) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
		return;
	}

	req_body body;

	// parse body
	std::string s_body = httpReq->read_body();
	if (s_body.empty()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
	if (!ok) {
		std::stringstream ss;
		ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
		return;
	}

	if (!doc.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
		return;
	}

	std::string strerror;
	if (!parse_req_params(doc, body, strerror)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}

	const rapidjson::Value& v_additional = doc["additional"];
	if (!v_additional.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}

	// image_filename
	std::string image_filename;
	JSON_PARSE_STRING(v_additional, "image_filename", image_filename);

	// 从client节点上传镜像到镜像中心(默认路径: /data/)
	if (!has_peer_nodeid(body)) {
		if (!boost::filesystem::path(image_filename).is_absolute())
			image_filename = "/data/" + image_filename;

		if (image_filename.empty() || !boost::filesystem::exists(image_filename)) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "image_filename: /data/" + image_filename + " not exist");
			return;
		}

		float progress = ImageManager::instance().uploadProgress(image_filename);

		std::string rsp_json = "{";
		rsp_json += "\"errcode\":0";
		rsp_json += ",\"message\":";
		rsp_json += "{";
		if (progress < 1.0f) {
			rsp_json += "\"status\": \"uploading\"";
			rsp_json += ",\"progress\": \"" + std::to_string(int(progress * 100)) + "%\"";
		}
		else {
			rsp_json += "\"status\": \"done\"";
			rsp_json += ",\"progress\": \"100%\"";
		}
		rsp_json += "}";
		rsp_json += "}";

		httpReq->reply_comm_rest_succ2(rsp_json);
	}
	// 从node_id指定的算力节点上传镜像到镜像中心
	else {
		// session_id wallet
		if (body.session_id.empty() || body.session_id_sign.empty()) {
			if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
				httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "session_id and wallet all invalid");
				return;
			}
		}

		// node request
		std::string head_session_id = util::create_session_id();

		auto node_req_msg = create_node_upload_image_progress_req_msg(head_session_id, body);
		if (nullptr == node_req_msg) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
			return;
		}

		// timer
		uint32_t timer_id = add_timer(NODE_UPLOAD_IMAGE_PROGRESS_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
			CALLBACK_1(rest_api_service::on_node_upload_image_progress_timer, this));
		if (INVALID_TIMER_ID == timer_id) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
			return;
		}

		// session
		if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id,
			body.peer_nodes_list[0])) {
			remove_timer(timer_id);
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
			return;
		}

		// broadcast message
		if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
			return;
		}
	}
}

std::shared_ptr<network::message>
rest_api_service::create_node_upload_image_progress_req_msg(const std::string& head_session_id, const req_body& body) {
	// 创建 node_ 请求
	auto req_content = std::make_shared<dbc::node_upload_image_progress_req>();
	// header
	req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
	req_content->header.__set_msg_name(NODE_UPLOAD_IMAGE_PROGRESS_REQ);
	req_content->header.__set_nonce(util::create_nonce());
	req_content->header.__set_session_id(head_session_id);
	std::map<std::string, std::string> exten_info;
	exten_info["pub_key"] = body.pub_key;
	req_content->header.__set_exten_info(exten_info);
	std::vector<std::string> path;
	path.push_back(ConfManager::instance().GetNodeId());
	req_content->header.__set_path(path);
	// body
	dbc::node_upload_image_progress_req_data req_data;
	req_data.__set_peer_nodes_list(body.peer_nodes_list);
	req_data.__set_additional(body.additional);
	req_data.__set_wallet(body.wallet);
	req_data.__set_nonce(body.nonce);
	req_data.__set_sign(body.sign);
	req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
	req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
	std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
	for (auto& it : body.multisig_accounts.signs) {
		dbc::multisig_sign_item item;
		item.wallet = it.wallet;
		item.nonce = it.nonce;
		item.sign = it.sign;
		vecMultisigSignItem.push_back(item);
	}
	req_data.__set_multisig_signs(vecMultisigSignItem);
	req_data.__set_session_id(body.session_id);
	req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

	// encrypt
	std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
	network::binary_protocol proto(out_buf.get());
	req_data.write(&proto);

	auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
	if (service_info != nullptr) {
		std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
		std::string priv_key = ConfManager::instance().GetPrivKey();

		if (!pub_key.empty() && !priv_key.empty()) {
			std::string s_data = encrypt_data((unsigned char*)out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
			req_content->body.__set_data(s_data);
		}
		else {
			LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
			return nullptr;
		}
	}
	else {
		LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
		return nullptr;
	}

	std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
	req_msg->set_name(NODE_UPLOAD_IMAGE_PROGRESS_REQ);
	req_msg->set_content(req_content);

	return req_msg;
}

void rest_api_service::on_node_upload_image_progress_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
	const std::shared_ptr<network::message>& rsp_msg) {
	auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_upload_image_progress_rsp>(rsp_msg->content);
	if (!node_rsp_msg) {
		LOG_ERROR << "node_rsp_msg is nullptr";
		return;
	}

	const std::shared_ptr<network::http_request>& httpReq = hreq_context->m_hreq;

	// decrypt
	std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
	std::string priv_key = ConfManager::instance().GetPrivKey();
	if (pub_key.empty() || priv_key.empty()) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
		LOG_ERROR << "pub_key or priv_key is empty";
		return;
	}

	std::string ori_message;
	try {
		bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
		if (!succ || ori_message.empty()) {
			httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error1");
			LOG_ERROR << "rsp decrypt error1";
			return;
		}
	}
	catch (std::exception& e) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error2");
		LOG_ERROR << "rsp decrypt error2";
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
	if (!ok) {
		LOG_ERROR << "http response parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "http response parse error");
		return;
	}

	httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_upload_image_progress_timer(const std::shared_ptr<core_timer>& timer) {
	if (nullptr == timer) {
		LOG_ERROR << "timer is nullptr";
		return;
	}

	const std::string& session_id = timer->get_session_id();
	std::shared_ptr<service_session> session = get_session(session_id);
	if (nullptr == session) {
		LOG_ERROR << "session is nullptr";
		return;
	}

	variables_map& vm = session->get_context().get_args();
	if (0 == vm.count(HTTP_REQUEST_KEY)) {
		LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
		session->clear();
		this->remove_session(session_id);
		return;
	}

	auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
	if (nullptr != hreq_context) {
		LOG_ERROR << "query upload image progress timeout";
		hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "query upload image progress timeout");
	}

	session->clear();
	this->remove_session(session_id);
}

// stop upload image
void rest_api_service::rest_stop_upload_image(const std::shared_ptr<network::http_request>& httpReq,
	const std::string& path) {
	if (httpReq->get_request_method() != network::http_request::POST) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
		return;
	}

	req_body body;

	// parse body
	std::string s_body = httpReq->read_body();
	if (s_body.empty()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
	if (!ok) {
		std::stringstream ss;
		ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
		return;
	}

	if (!doc.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
		return;
	}

	std::string strerror;
	if (!parse_req_params(doc, body, strerror)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}

	const rapidjson::Value& v_additional = doc["additional"];
	if (!v_additional.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}

	// image_filename
	std::string image_filename;
	JSON_PARSE_STRING(v_additional, "image_filename", image_filename);

	// 从client节点上传镜像到镜像中心(默认路径: /data/)
	if (!has_peer_nodeid(body)) {
		if (!boost::filesystem::path(image_filename).is_absolute())
			image_filename = "/data/" + image_filename;

		if (image_filename.empty() || !boost::filesystem::exists(image_filename)) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "image_filename: /data/" + image_filename + " not exist");
			return;
		}

		ImageManager::instance().terminateUpload(image_filename);

		std::string rsp_json = "{";
		rsp_json += "\"errcode\":0";
		rsp_json += ",\"message\":\"ok\"";
		rsp_json += "}";

		httpReq->reply_comm_rest_succ2(rsp_json);
	}
	// 从node_id指定的算力节点上传镜像到镜像中心
	else {
		// session_id wallet
		if (body.session_id.empty() || body.session_id_sign.empty()) {
			if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
				httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "session_id and wallet all invalid");
				return;
			}
		}

		// node request
		std::string head_session_id = util::create_session_id();

		auto node_req_msg = create_node_stop_upload_image_req_msg(head_session_id, body);
		if (nullptr == node_req_msg) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
			return;
		}

		// timer
		uint32_t timer_id = add_timer(NODE_STOP_UPLOAD_IMAGE_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
			CALLBACK_1(rest_api_service::on_node_stop_upload_image_timer, this));
		if (INVALID_TIMER_ID == timer_id) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
			return;
		}

		// session
		if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id,
			body.peer_nodes_list[0])) {
			remove_timer(timer_id);
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
			return;
		}

		// broadcast message
		if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
			return;
		}
	}
}

std::shared_ptr<network::message>
rest_api_service::create_node_stop_upload_image_req_msg(const std::string& head_session_id, const req_body& body) {
	// 创建 node_ 请求
	auto req_content = std::make_shared<dbc::node_stop_upload_image_req>();
	// header
	req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
	req_content->header.__set_msg_name(NODE_STOP_UPLOAD_IMAGE_REQ);
	req_content->header.__set_nonce(util::create_nonce());
	req_content->header.__set_session_id(head_session_id);
	std::map<std::string, std::string> exten_info;
	exten_info["pub_key"] = body.pub_key;
	req_content->header.__set_exten_info(exten_info);
	std::vector<std::string> path;
	path.push_back(ConfManager::instance().GetNodeId());
	req_content->header.__set_path(path);
	// body
	dbc::node_upload_image_progress_req_data req_data;
	req_data.__set_peer_nodes_list(body.peer_nodes_list);
	req_data.__set_additional(body.additional);
	req_data.__set_wallet(body.wallet);
	req_data.__set_nonce(body.nonce);
	req_data.__set_sign(body.sign);
	req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
	req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
	std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
	for (auto& it : body.multisig_accounts.signs) {
		dbc::multisig_sign_item item;
		item.wallet = it.wallet;
		item.nonce = it.nonce;
		item.sign = it.sign;
		vecMultisigSignItem.push_back(item);
	}
	req_data.__set_multisig_signs(vecMultisigSignItem);
	req_data.__set_session_id(body.session_id);
	req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

	// encrypt
	std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
	network::binary_protocol proto(out_buf.get());
	req_data.write(&proto);

	auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
	if (service_info != nullptr) {
		std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
		std::string priv_key = ConfManager::instance().GetPrivKey();

		if (!pub_key.empty() && !priv_key.empty()) {
			std::string s_data = encrypt_data((unsigned char*)out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
			req_content->body.__set_data(s_data);
		}
		else {
			LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
			return nullptr;
		}
	}
	else {
		LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
		return nullptr;
	}

	std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
	req_msg->set_name(NODE_STOP_UPLOAD_IMAGE_REQ);
	req_msg->set_content(req_content);

	return req_msg;
}

void rest_api_service::on_node_stop_upload_image_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
	const std::shared_ptr<network::message>& rsp_msg) {
	auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_stop_upload_image_rsp>(rsp_msg->content);
	if (!node_rsp_msg) {
		LOG_ERROR << "node_rsp_msg is nullptr";
		return;
	}

	const std::shared_ptr<network::http_request>& httpReq = hreq_context->m_hreq;

	// decrypt
	std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
	std::string priv_key = ConfManager::instance().GetPrivKey();
	if (pub_key.empty() || priv_key.empty()) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
		LOG_ERROR << "pub_key or priv_key is empty";
		return;
	}

	std::string ori_message;
	try {
		bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
		if (!succ || ori_message.empty()) {
			httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error1");
			LOG_ERROR << "rsp decrypt error1";
			return;
		}
	}
	catch (std::exception& e) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error2");
		LOG_ERROR << "rsp decrypt error2";
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
	if (!ok) {
		LOG_ERROR << "http response parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "http response parse error");
		return;
	}

	httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_stop_upload_image_timer(const std::shared_ptr<core_timer>& timer) {
	if (nullptr == timer) {
		LOG_ERROR << "timer is nullptr";
		return;
	}

	const std::string& session_id = timer->get_session_id();
	std::shared_ptr<service_session> session = get_session(session_id);
	if (nullptr == session) {
		LOG_ERROR << "session is nullptr";
		return;
	}

	variables_map& vm = session->get_context().get_args();
	if (0 == vm.count(HTTP_REQUEST_KEY)) {
		LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
		session->clear();
		this->remove_session(session_id);
		return;
	}

	auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
	if (nullptr != hreq_context) {
		LOG_ERROR << "stop upload image timeout";
		hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "upload image timeout");
	}

	session->clear();
	this->remove_session(session_id);
}

// delete image
void rest_api_service::rest_delete_image(const std::shared_ptr<network::http_request>& httpReq,
	const std::string& path) {
	if (httpReq->get_request_method() != network::http_request::POST) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
		return;
	}

	req_body body;

	// parse body
	std::string s_body = httpReq->read_body();
	if (s_body.empty()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
	if (!ok) {
		std::stringstream ss;
		ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
		return;
	}

	if (!doc.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
		return;
	}

	std::string strerror;
	if (!parse_req_params(doc, body, strerror)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}

	const rapidjson::Value& v_additional = doc["additional"];
	if (!v_additional.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}

	// image_filename
	std::string image_filename;
	JSON_PARSE_STRING(v_additional, "image_filename", image_filename);

	// peer_nodeid
	if (!has_peer_nodeid(body)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
		return;
	}

	// session_id wallet
	if (body.session_id.empty() || body.session_id_sign.empty()) {
		if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "session_id and wallet all invalid");
			return;
		}
	}

	// node request
	std::string head_session_id = util::create_session_id();

	auto node_req_msg = create_node_delete_image_req_msg(head_session_id, body);
	if (nullptr == node_req_msg) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
		return;
	}

	// timer
	uint32_t timer_id = add_timer(NODE_DELETE_IMAGE_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
		CALLBACK_1(rest_api_service::on_node_delete_image_timer, this));
	if (INVALID_TIMER_ID == timer_id) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
		return;
	}

	// session
	if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id,
		body.peer_nodes_list[0])) {
		remove_timer(timer_id);
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
		return;
	}

	// broadcast message
	if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
		return;
	}
}

std::shared_ptr<network::message>
rest_api_service::create_node_delete_image_req_msg(const std::string& head_session_id, const req_body& body) {
	// 创建 node_ 请求
	auto req_content = std::make_shared<dbc::node_delete_image_req>();
	// header
	req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
	req_content->header.__set_msg_name(NODE_DELETE_IMAGE_REQ);
	req_content->header.__set_nonce(util::create_nonce());
	req_content->header.__set_session_id(head_session_id);
	std::map<std::string, std::string> exten_info;
	exten_info["pub_key"] = body.pub_key;
	req_content->header.__set_exten_info(exten_info);
	std::vector<std::string> path;
	path.push_back(ConfManager::instance().GetNodeId());
	req_content->header.__set_path(path);
	// body
	dbc::node_upload_image_req_data req_data;
	req_data.__set_peer_nodes_list(body.peer_nodes_list);
	req_data.__set_additional(body.additional);
	req_data.__set_wallet(body.wallet);
	req_data.__set_nonce(body.nonce);
	req_data.__set_sign(body.sign);
	req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
	req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
	std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
	for (auto& it : body.multisig_accounts.signs) {
		dbc::multisig_sign_item item;
		item.wallet = it.wallet;
		item.nonce = it.nonce;
		item.sign = it.sign;
		vecMultisigSignItem.push_back(item);
	}
	req_data.__set_multisig_signs(vecMultisigSignItem);
	req_data.__set_session_id(body.session_id);
	req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

	// encrypt
	std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
	network::binary_protocol proto(out_buf.get());
	req_data.write(&proto);

	auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
	if (service_info != nullptr) {
		std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
		std::string priv_key = ConfManager::instance().GetPrivKey();

		if (!pub_key.empty() && !priv_key.empty()) {
			std::string s_data = encrypt_data((unsigned char*)out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
			req_content->body.__set_data(s_data);
		}
		else {
			LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
			return nullptr;
		}
	}
	else {
		LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
		return nullptr;
	}

	std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
	req_msg->set_name(NODE_DELETE_IMAGE_REQ);
	req_msg->set_content(req_content);

	return req_msg;
}

void rest_api_service::on_node_delete_image_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
	const std::shared_ptr<network::message>& rsp_msg) {
	auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_delete_image_rsp>(rsp_msg->content);
	if (!node_rsp_msg) {
		LOG_ERROR << "node_rsp_msg is nullptr";
		return;
	}

	const std::shared_ptr<network::http_request>& httpReq = hreq_context->m_hreq;

	// decrypt
	std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
	std::string priv_key = ConfManager::instance().GetPrivKey();
	if (pub_key.empty() || priv_key.empty()) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
		LOG_ERROR << "pub_key or priv_key is empty";
		return;
	}

	std::string ori_message;
	try {
		bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
		if (!succ || ori_message.empty()) {
			httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error1");
			LOG_ERROR << "rsp decrypt error1";
			return;
		}
	}
	catch (std::exception& e) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error2");
		LOG_ERROR << "rsp decrypt error2";
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
	if (!ok) {
		LOG_ERROR << "http response parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "http response parse error");
		return;
	}

	httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_delete_image_timer(const std::shared_ptr<core_timer>& timer) {
	if (nullptr == timer) {
		LOG_ERROR << "timer is nullptr";
		return;
	}

	const std::string& session_id = timer->get_session_id();
	std::shared_ptr<service_session> session = get_session(session_id);
	if (nullptr == session) {
		LOG_ERROR << "session is nullptr";
		return;
	}

	variables_map& vm = session->get_context().get_args();
	if (0 == vm.count(HTTP_REQUEST_KEY)) {
		LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
		session->clear();
		this->remove_session(session_id);
		return;
	}

	auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
	if (nullptr != hreq_context) {
		LOG_ERROR << "delete image timeout";
		hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "upload image timeout");
	}

	session->clear();
	this->remove_session(session_id);
}

// /disk
void rest_api_service::rest_disk(const std::shared_ptr<network::http_request>& httpReq, const std::string& path) {
	std::vector<std::string> path_list;
	util::split_path(path, path_list);

	if (path_list.size() == 2) {
		const std::string& second_param = path_list[0];
		if (second_param == "list") {
			rest_list_disk(httpReq, path);
			return;
		}
		else if (second_param == "resize") {
			rest_resize_disk(httpReq, path);
			return;
		}
		else if (second_param == "add") {
			rest_add_disk(httpReq, path);
			return;
		}
		else if (second_param == "delete") {
			rest_delete_disk(httpReq, path);
			return;
		}
	}

	httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "invalid requests uri");
}

// list disk
void rest_api_service::rest_list_disk(const std::shared_ptr<network::http_request>& httpReq, const std::string& path) {
	if (httpReq->get_request_method() != network::http_request::POST) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
		return;
	}

	std::vector<std::string> path_list;
	util::split_path(path, path_list);
	if (path_list.size() != 2) {
		LOG_ERROR << "path_list's size != 2";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /disk/list/<task_id>");
		return;
	}

	req_body body;

	body.task_id = path_list[1];
	if (body.task_id.empty()) {
		LOG_ERROR << "task_id is empty";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "task_id is empty");
		return;
	}

	std::string s_body = httpReq->read_body();
	if (s_body.empty()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
	if (!ok) {
		std::stringstream ss;
		ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
		return;
	}

	if (!doc.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
		return;
	}

	std::string strerror;
	if (!parse_req_params(doc, body, strerror)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}
    
	// peer_nodeid
	if (!has_peer_nodeid(body)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
		return;
	}

	// session_id wallet
	if (body.session_id.empty() || body.session_id_sign.empty()) {
		if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "session_id and wallet all invalid");
			return;
		}
	}

	// node request
	std::string head_session_id = util::create_session_id();

	auto node_req_msg = create_node_list_disk_req_msg(head_session_id, body);
	if (nullptr == node_req_msg) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
		return;
	}

	// timer
	uint32_t timer_id = add_timer(NODE_LIST_DISK_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
		CALLBACK_1(rest_api_service::on_node_list_disk_timer, this));
	if (INVALID_TIMER_ID == timer_id) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
		return;
	}

	// session
	if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id,
		body.peer_nodes_list[0])) {
		remove_timer(timer_id);
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
		return;
	}

	// broadcast message
	if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
		return;
	}
}

std::shared_ptr<network::message> rest_api_service::create_node_list_disk_req_msg(const std::string& head_session_id,
    const req_body& body) {
	auto req_content = std::make_shared<dbc::node_list_disk_req>();
	// header
	req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
	req_content->header.__set_msg_name(NODE_LIST_DISK_REQ);
	req_content->header.__set_nonce(util::create_nonce());
	req_content->header.__set_session_id(head_session_id);
	std::map<std::string, std::string> exten_info;
	exten_info["pub_key"] = body.pub_key;
	req_content->header.__set_exten_info(exten_info);
	std::vector<std::string> path;
	path.push_back(ConfManager::instance().GetNodeId());
	req_content->header.__set_path(path);
	// body
	dbc::node_list_disk_req_data req_data;
    req_data.__set_task_id(body.task_id);
	req_data.__set_peer_nodes_list(body.peer_nodes_list);
	req_data.__set_additional(body.additional);
	req_data.__set_wallet(body.wallet);
	req_data.__set_nonce(body.nonce);
	req_data.__set_sign(body.sign);
	req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
	req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
	std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
	for (auto& it : body.multisig_accounts.signs) {
		dbc::multisig_sign_item item;
		item.wallet = it.wallet;
		item.nonce = it.nonce;
		item.sign = it.sign;
		vecMultisigSignItem.push_back(item);
	}
	req_data.__set_multisig_signs(vecMultisigSignItem);
	req_data.__set_session_id(body.session_id);
	req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

	// encrypt
	std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
	network::binary_protocol proto(out_buf.get());
	req_data.write(&proto);

	auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
	if (service_info != nullptr) {
		std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
		std::string priv_key = ConfManager::instance().GetPrivKey();

		if (!pub_key.empty() && !priv_key.empty()) {
			std::string s_data = encrypt_data((unsigned char*)out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
			req_content->body.__set_data(s_data);
		}
		else {
			LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
			return nullptr;
		}
	}
	else {
		LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
		return nullptr;
	}

	std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
	req_msg->set_name(NODE_LIST_DISK_REQ);
	req_msg->set_content(req_content);

	return req_msg;
}

void rest_api_service::on_node_list_disk_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
    const std::shared_ptr<network::message>& rsp_msg) {
	auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_list_disk_rsp>(rsp_msg->content);
	if (!node_rsp_msg) {
		LOG_ERROR << "node_rsp_msg is nullptr";
		return;
	}

	const std::shared_ptr<network::http_request>& httpReq = hreq_context->m_hreq;

	// decrypt
	std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
	std::string priv_key = ConfManager::instance().GetPrivKey();
	if (pub_key.empty() || priv_key.empty()) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
		LOG_ERROR << "pub_key or priv_key is empty";
		return;
	}

	std::string ori_message;
	try {
		bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
		if (!succ || ori_message.empty()) {
			httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error1");
			LOG_ERROR << "rsp decrypt error1";
			return;
		}
	}
	catch (std::exception& e) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error2");
		LOG_ERROR << "rsp decrypt error2";
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
	if (!ok) {
		LOG_ERROR << "http response parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "http response parse error");
		return;
	}

	httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_list_disk_timer(const std::shared_ptr<core_timer>& timer) {
	if (nullptr == timer) {
		LOG_ERROR << "timer is nullptr";
		return;
	}

	const std::string& session_id = timer->get_session_id();
	std::shared_ptr<service_session> session = get_session(session_id);
	if (nullptr == session) {
		LOG_ERROR << "session is nullptr";
		return;
	}

	variables_map& vm = session->get_context().get_args();
	if (0 == vm.count(HTTP_REQUEST_KEY)) {
		LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
		session->clear();
		this->remove_session(session_id);
		return;
	}

	auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
	if (nullptr != hreq_context) {
		hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "list disk timeout");
	}

	session->clear();
	this->remove_session(session_id);
}

// resize disk
void rest_api_service::rest_resize_disk(const std::shared_ptr<network::http_request>& httpReq, const std::string& path) {
	if (httpReq->get_request_method() != network::http_request::POST) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
		return;
	}

	std::vector<std::string> path_list;
	util::split_path(path, path_list);
	if (path_list.size() != 2) {
		LOG_ERROR << "path_list's size != 2";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /disk/resize/<task_id>");
		return;
	}

	req_body body;

	body.task_id = path_list[1];
	if (body.task_id.empty()) {
		LOG_ERROR << "task_id is empty";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "task_id is empty");
		return;
	}

	// parse body
	std::string s_body = httpReq->read_body();
	if (s_body.empty()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
	if (!ok) {
		std::stringstream ss;
		ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
		return;
	}

	if (!doc.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
		return;
	}

	std::string strerror;
	if (!parse_req_params(doc, body, strerror)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}

	// peer_nodeid
	if (!has_peer_nodeid(body)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
		return;
	}

	// session_id wallet
	if (body.session_id.empty() || body.session_id_sign.empty()) {
		if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "session_id and wallet all invalid");
			return;
		}
	}

	// node request
	std::string head_session_id = util::create_session_id();

	auto node_req_msg = create_node_resize_disk_req_msg(head_session_id, body);
	if (nullptr == node_req_msg) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
		return;
	}

	// timer
	uint32_t timer_id = add_timer(NODE_RESIZE_DISK_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
		CALLBACK_1(rest_api_service::on_node_resize_disk_timer, this));
	if (INVALID_TIMER_ID == timer_id) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
		return;
	}

	// session
	if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id,
		body.peer_nodes_list[0])) {
		remove_timer(timer_id);
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
		return;
	}

	// broadcast message
	if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
		return;
	}
}

std::shared_ptr<network::message> rest_api_service::create_node_resize_disk_req_msg(const std::string& head_session_id,
    const req_body& body) {
	auto req_content = std::make_shared<dbc::node_resize_disk_req>();
	// header
	req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
	req_content->header.__set_msg_name(NODE_RESIZE_DISK_REQ);
	req_content->header.__set_nonce(util::create_nonce());
	req_content->header.__set_session_id(head_session_id);
	std::map<std::string, std::string> exten_info;
	exten_info["pub_key"] = body.pub_key;
	req_content->header.__set_exten_info(exten_info);
	std::vector<std::string> path;
	path.push_back(ConfManager::instance().GetNodeId());
	req_content->header.__set_path(path);
	// body
	dbc::node_list_disk_req_data req_data;
    req_data.__set_task_id(body.task_id);
	req_data.__set_peer_nodes_list(body.peer_nodes_list);
	req_data.__set_additional(body.additional);
	req_data.__set_wallet(body.wallet);
	req_data.__set_nonce(body.nonce);
	req_data.__set_sign(body.sign);
	req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
	req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
	std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
	for (auto& it : body.multisig_accounts.signs) {
		dbc::multisig_sign_item item;
		item.wallet = it.wallet;
		item.nonce = it.nonce;
		item.sign = it.sign;
		vecMultisigSignItem.push_back(item);
	}
	req_data.__set_multisig_signs(vecMultisigSignItem);
	req_data.__set_session_id(body.session_id);
	req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

	// encrypt
	std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
	network::binary_protocol proto(out_buf.get());
	req_data.write(&proto);

	auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
	if (service_info != nullptr) {
		std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
		std::string priv_key = ConfManager::instance().GetPrivKey();

		if (!pub_key.empty() && !priv_key.empty()) {
			std::string s_data = encrypt_data((unsigned char*)out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
			req_content->body.__set_data(s_data);
		}
		else {
			LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
			return nullptr;
		}
	}
	else {
		LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
		return nullptr;
	}

	std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
	req_msg->set_name(NODE_RESIZE_DISK_REQ);
	req_msg->set_content(req_content);

	return req_msg;
}

void rest_api_service::on_node_resize_disk_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
    const std::shared_ptr<network::message>& rsp_msg) {
	auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_resize_disk_rsp>(rsp_msg->content);
	if (!node_rsp_msg) {
		LOG_ERROR << "node_rsp_msg is nullptr";
		return;
	}

	const std::shared_ptr<network::http_request>& httpReq = hreq_context->m_hreq;

	// decrypt
	std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
	std::string priv_key = ConfManager::instance().GetPrivKey();
	if (pub_key.empty() || priv_key.empty()) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
		LOG_ERROR << "pub_key or priv_key is empty";
		return;
	}

	std::string ori_message;
	try {
		bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
		if (!succ || ori_message.empty()) {
			httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error1");
			LOG_ERROR << "rsp decrypt error1";
			return;
		}
	}
	catch (std::exception& e) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error2");
		LOG_ERROR << "rsp decrypt error2";
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
	if (!ok) {
		LOG_ERROR << "http response parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "http response parse error");
		return;
	}

	httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_resize_disk_timer(const std::shared_ptr<core_timer>& timer) {
	if (nullptr == timer) {
		LOG_ERROR << "timer is nullptr";
		return;
	}

	const std::string& session_id = timer->get_session_id();
	std::shared_ptr<service_session> session = get_session(session_id);
	if (nullptr == session) {
		LOG_ERROR << "session is nullptr";
		return;
	}

	variables_map& vm = session->get_context().get_args();
	if (0 == vm.count(HTTP_REQUEST_KEY)) {
		LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
		session->clear();
		this->remove_session(session_id);
		return;
	}

	auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
	if (nullptr != hreq_context) {
		hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "resize disk timeout");
	}

	session->clear();
	this->remove_session(session_id);
}

// add disk
void rest_api_service::rest_add_disk(const std::shared_ptr<network::http_request>& httpReq, const std::string& path) {
	if (httpReq->get_request_method() != network::http_request::POST) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
		return;
	}

	std::vector<std::string> path_list;
	util::split_path(path, path_list);
	if (path_list.size() != 2) {
		LOG_ERROR << "path_list's size != 2";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /disk/resize/<task_id>");
		return;
	}

	req_body body;

	body.task_id = path_list[1];
	if (body.task_id.empty()) {
		LOG_ERROR << "task_id is empty";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "task_id is empty");
		return;
	}

	// parse body
	std::string s_body = httpReq->read_body();
	if (s_body.empty()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
	if (!ok) {
		std::stringstream ss;
		ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
		return;
	}

	if (!doc.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
		return;
	}

	std::string strerror;
	if (!parse_req_params(doc, body, strerror)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}

	// peer_nodeid
	if (!has_peer_nodeid(body)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
		return;
	}

	// session_id wallet
	if (body.session_id.empty() || body.session_id_sign.empty()) {
		if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "session_id and wallet all invalid");
			return;
		}
	}

	// node request
	std::string head_session_id = util::create_session_id();

	auto node_req_msg = create_node_add_disk_req_msg(head_session_id, body);
	if (nullptr == node_req_msg) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
		return;
	}

	// timer
	uint32_t timer_id = add_timer(NODE_ADD_DISK_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
		CALLBACK_1(rest_api_service::on_node_add_disk_timer, this));
	if (INVALID_TIMER_ID == timer_id) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
		return;
	}

	// session
	if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id,
		body.peer_nodes_list[0])) {
		remove_timer(timer_id);
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
		return;
	}

	// broadcast message
	if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
		return;
	}
}

std::shared_ptr<network::message> rest_api_service::create_node_add_disk_req_msg(const std::string& head_session_id,
    const req_body& body) {
	auto req_content = std::make_shared<dbc::node_add_disk_req>();
	// header
	req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
	req_content->header.__set_msg_name(NODE_ADD_DISK_REQ);
	req_content->header.__set_nonce(util::create_nonce());
	req_content->header.__set_session_id(head_session_id);
	std::map<std::string, std::string> exten_info;
	exten_info["pub_key"] = body.pub_key;
	req_content->header.__set_exten_info(exten_info);
	std::vector<std::string> path;
	path.push_back(ConfManager::instance().GetNodeId());
	req_content->header.__set_path(path);
	// body
	dbc::node_add_disk_req_data req_data;
    req_data.__set_task_id(body.task_id);
	req_data.__set_peer_nodes_list(body.peer_nodes_list);
	req_data.__set_additional(body.additional);
	req_data.__set_wallet(body.wallet);
	req_data.__set_nonce(body.nonce);
	req_data.__set_sign(body.sign);
	req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
	req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
	std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
	for (auto& it : body.multisig_accounts.signs) {
		dbc::multisig_sign_item item;
		item.wallet = it.wallet;
		item.nonce = it.nonce;
		item.sign = it.sign;
		vecMultisigSignItem.push_back(item);
	}
	req_data.__set_multisig_signs(vecMultisigSignItem);
	req_data.__set_session_id(body.session_id);
	req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

	// encrypt
	std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
	network::binary_protocol proto(out_buf.get());
	req_data.write(&proto);

	auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
	if (service_info != nullptr) {
		std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
		std::string priv_key = ConfManager::instance().GetPrivKey();

		if (!pub_key.empty() && !priv_key.empty()) {
			std::string s_data = encrypt_data((unsigned char*)out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
			req_content->body.__set_data(s_data);
		}
		else {
			LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
			return nullptr;
		}
	}
	else {
		LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
		return nullptr;
	}

	std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
	req_msg->set_name(NODE_ADD_DISK_REQ);
	req_msg->set_content(req_content);

	return req_msg;
}

void rest_api_service::on_node_add_disk_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
    const std::shared_ptr<network::message>& rsp_msg) {
	auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_add_disk_rsp>(rsp_msg->content);
	if (!node_rsp_msg) {
		LOG_ERROR << "node_rsp_msg is nullptr";
		return;
	}

	const std::shared_ptr<network::http_request>& httpReq = hreq_context->m_hreq;

	// decrypt
	std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
	std::string priv_key = ConfManager::instance().GetPrivKey();
	if (pub_key.empty() || priv_key.empty()) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
		LOG_ERROR << "pub_key or priv_key is empty";
		return;
	}

	std::string ori_message;
	try {
		bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
		if (!succ || ori_message.empty()) {
			httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error1");
			LOG_ERROR << "rsp decrypt error1";
			return;
		}
	}
	catch (std::exception& e) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error2");
		LOG_ERROR << "rsp decrypt error2";
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
	if (!ok) {
		LOG_ERROR << "http response parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "http response parse error");
		return;
	}

	httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_add_disk_timer(const std::shared_ptr<core_timer>& timer) {
	if (nullptr == timer) {
		LOG_ERROR << "timer is nullptr";
		return;
	}

	const std::string& session_id = timer->get_session_id();
	std::shared_ptr<service_session> session = get_session(session_id);
	if (nullptr == session) {
		LOG_ERROR << "session is nullptr";
		return;
	}

	variables_map& vm = session->get_context().get_args();
	if (0 == vm.count(HTTP_REQUEST_KEY)) {
		LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
		session->clear();
		this->remove_session(session_id);
		return;
	}

	auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
	if (nullptr != hreq_context) {
		hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "add disk timeout");
	}

	session->clear();
	this->remove_session(session_id);
}

// delete disk
void rest_api_service::rest_delete_disk(const std::shared_ptr<network::http_request>& httpReq, const std::string& path) {
	if (httpReq->get_request_method() != network::http_request::POST) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
		return;
	}

	std::vector<std::string> path_list;
	util::split_path(path, path_list);
	if (path_list.size() != 2) {
		LOG_ERROR << "path_list's size != 2";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /disk/resize/<task_id>");
		return;
	}

	req_body body;

	body.task_id = path_list[1];
	if (body.task_id.empty()) {
		LOG_ERROR << "task_id is empty";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "task_id is empty");
		return;
	}

	// parse body
	std::string s_body = httpReq->read_body();
	if (s_body.empty()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
	if (!ok) {
		std::stringstream ss;
		ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
		return;
	}

	if (!doc.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
		return;
	}

	std::string strerror;
	if (!parse_req_params(doc, body, strerror)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}

	// peer_nodeid
	if (!has_peer_nodeid(body)) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
		return;
	}

	// session_id wallet
	if (body.session_id.empty() || body.session_id_sign.empty()) {
		if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "session_id and wallet all invalid");
			return;
		}
	}

	// node request
	std::string head_session_id = util::create_session_id();

	auto node_req_msg = create_node_delete_disk_req_msg(head_session_id, body);
	if (nullptr == node_req_msg) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
		return;
	}

	// timer
	uint32_t timer_id = add_timer(NODE_DELETE_DISK_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
		CALLBACK_1(rest_api_service::on_node_delete_disk_timer, this));
	if (INVALID_TIMER_ID == timer_id) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
		return;
	}

	// session
	if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id,
		body.peer_nodes_list[0])) {
		remove_timer(timer_id);
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
		return;
	}

	// broadcast message
	if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
		return;
	}
}

std::shared_ptr<network::message> rest_api_service::create_node_delete_disk_req_msg(const std::string& head_session_id,
    const req_body& body) {
	auto req_content = std::make_shared<dbc::node_delete_disk_req>();
	// header
	req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
	req_content->header.__set_msg_name(NODE_DELETE_DISK_REQ);
	req_content->header.__set_nonce(util::create_nonce());
	req_content->header.__set_session_id(head_session_id);
	std::map<std::string, std::string> exten_info;
	exten_info["pub_key"] = body.pub_key;
	req_content->header.__set_exten_info(exten_info);
	std::vector<std::string> path;
	path.push_back(ConfManager::instance().GetNodeId());
	req_content->header.__set_path(path);
	// body
	dbc::node_add_disk_req_data req_data;
    req_data.__set_task_id(body.task_id);
	req_data.__set_peer_nodes_list(body.peer_nodes_list);
	req_data.__set_additional(body.additional);
	req_data.__set_wallet(body.wallet);
	req_data.__set_nonce(body.nonce);
	req_data.__set_sign(body.sign);
	req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
	req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
	std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
	for (auto& it : body.multisig_accounts.signs) {
		dbc::multisig_sign_item item;
		item.wallet = it.wallet;
		item.nonce = it.nonce;
		item.sign = it.sign;
		vecMultisigSignItem.push_back(item);
	}
	req_data.__set_multisig_signs(vecMultisigSignItem);
	req_data.__set_session_id(body.session_id);
	req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

	// encrypt
	std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
	network::binary_protocol proto(out_buf.get());
	req_data.write(&proto);

	auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
	if (service_info != nullptr) {
		std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
		std::string priv_key = ConfManager::instance().GetPrivKey();

		if (!pub_key.empty() && !priv_key.empty()) {
			std::string s_data = encrypt_data((unsigned char*)out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
			req_content->body.__set_data(s_data);
		}
		else {
			LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
			return nullptr;
		}
	}
	else {
		LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
		return nullptr;
	}

	std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
	req_msg->set_name(NODE_DELETE_DISK_REQ);
	req_msg->set_content(req_content);

	return req_msg;
}

void rest_api_service::on_node_delete_disk_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
    const std::shared_ptr<network::message>& rsp_msg) {
	auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_delete_disk_rsp>(rsp_msg->content);
	if (!node_rsp_msg) {
		LOG_ERROR << "node_rsp_msg is nullptr";
		return;
	}

	const std::shared_ptr<network::http_request>& httpReq = hreq_context->m_hreq;

	// decrypt
	std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
	std::string priv_key = ConfManager::instance().GetPrivKey();
	if (pub_key.empty() || priv_key.empty()) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
		LOG_ERROR << "pub_key or priv_key is empty";
		return;
	}

	std::string ori_message;
	try {
		bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
		if (!succ || ori_message.empty()) {
			httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error1");
			LOG_ERROR << "rsp decrypt error1";
			return;
		}
	}
	catch (std::exception& e) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error2");
		LOG_ERROR << "rsp decrypt error2";
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
	if (!ok) {
		LOG_ERROR << "http response parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "http response parse error");
		return;
	}

	httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_delete_disk_timer(const std::shared_ptr<core_timer>& timer) {
	if (nullptr == timer) {
		LOG_ERROR << "timer is nullptr";
		return;
	}

	const std::string& session_id = timer->get_session_id();
	std::shared_ptr<service_session> session = get_session(session_id);
	if (nullptr == session) {
		LOG_ERROR << "session is nullptr";
		return;
	}

	variables_map& vm = session->get_context().get_args();
	if (0 == vm.count(HTTP_REQUEST_KEY)) {
		LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
		session->clear();
		this->remove_session(session_id);
		return;
	}

	auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
	if (nullptr != hreq_context) {
		hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "delete disk timeout");
	}

	session->clear();
	this->remove_session(session_id);
}

// /mining_nodes
void rest_api_service::rest_mining_nodes(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.empty()) {
        rest_list_mining_nodes(httpReq, path);
        return;
    }

    if (path_list.size() == 1) {
        const std::string &first_param = path_list[0];
        if (first_param == "session_id") {
            rest_node_session_id(httpReq, path);
            return;
        }
        else if (first_param == "free_memory") {
            rest_free_memory(httpReq, path);
            return;
        }
    }

    httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "invalid requests uri");
}

// list mining node
void reply_node_list(const std::map<std::string, std::shared_ptr<dbc::node_service_info>>& service_list, std::string &data_json) {
    std::stringstream ss;
    ss << "{";
    ss << "\"errcode\":0";
    ss << ", \"message\":{";
    ss << "\"mining_nodes\":[";
    int service_count = 0;
    for (auto it : service_list) {
        if (service_count > 0) ss << ",";
        ss << "{";
        /*
        std::string service_list;
        for (int i = 0; i < it.second->service_list.size(); i++) {
            if (i > 0) service_list += ",";
            service_list += it.second->service_list[i];
        }
        ss << "\"service_list\":" << "\"" << service_list << "\"";
        */
        std::string node_id = it.first;
        node_id = util::rtrim(node_id, '\n');
        ss << "\"node_id\":" << "\"" << node_id << "\"";
        ss << ",\"name\":" << "\"" << it.second->name << "\"";
        std::string ver = it.second->kvs.count("version") ? it.second->kvs["version"] : "N/A";
        ss << ",\"version\":" << "\"" << ver << "\"";
        //ss << ",\"state\":" << "\"" << it.second.kvs["state"] << "\"";
        ss << "}";

        service_count++;
    }
    ss << "]}";
    ss << "}";

    data_json = ss.str();
}

void rest_api_service::rest_list_mining_nodes(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (!path_list.empty()) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /mining_nodes");
        return;
    }

    req_body body;

    // parse body
    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse req params failed: " + strerror);
        return;
    }

    // all peer_nodes
    if (body.peer_nodes_list.empty()) {
        auto& id_2_services = ServiceInfoManager::instance().get_all();
        std::string data_json;
        reply_node_list(id_2_services, data_json);
        httpReq->reply_comm_rest_succ2(data_json);
    } else {
		// image_server
		const rapidjson::Value& v_additional = doc["additional"];
		if (!v_additional.IsObject()) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
			return;
		}
		std::string image_server;
		JSON_PARSE_STRING(v_additional, "image_server", image_server)

		auto it_svr = ConfManager::instance().FindImageServer(image_server);
		if (!image_server.empty() && it_svr == nullptr) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "image_server not exist");
			return;
		}
        if (it_svr != nullptr)
    		body.image_server = it_svr->to_string();

        // node request
        std::string head_session_id = util::create_session_id();

        auto node_req_msg = create_node_query_node_info_req_msg(head_session_id, body);
        if (nullptr == node_req_msg) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
            return;
        }

        // timer
        uint32_t timer_id = add_timer(NODE_QUERY_NODE_INFO_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
            CALLBACK_1(rest_api_service::on_node_query_node_info_timer, this));
        if (INVALID_TIMER_ID == timer_id) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
            return;
        }

        // session
        if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
            remove_timer(timer_id);
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
            return;
        }

        // broadcast message
        if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
            return;
        }
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_query_node_info_req_msg(const std::string &head_session_id,
                                                                                             const req_body& body) {
    auto req_content = std::make_shared<dbc::node_query_node_info_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_QUERY_NODE_INFO_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_query_node_info_req_data req_data;
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_image_server(body.image_server);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    auto req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_QUERY_NODE_INFO_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_query_node_info_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
                                                      const std::shared_ptr<network::message>& rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_query_node_info_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error2");
        LOG_ERROR << "rsq decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_query_node_info_timer(const std::shared_ptr<core_timer>& timer)
{
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "query node info timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// query node rent orders
void rest_api_service::rest_rent_orders(
        const std::shared_ptr<network::http_request> &httpReq, const std::string &path) {
    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.size() <= 1) {
        rest_query_node_rent_orders(httpReq, path);
        return;
    }

    httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "invalid requests uri");
}

void rest_api_service::rest_query_node_rent_orders(
        const std::shared_ptr<network::http_request> &httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    req_body body;
    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() == 1) {
        body.rent_order = path_list[0];
    } else if (path_list.size() > 1) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /rent_orders");
        return;
    }

    // parse body
    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse req params failed: " + strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_query_node_rent_orders_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(QUERY_NODE_RENT_ORDERS_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_query_node_rent_orders_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_query_node_rent_orders_req_msg(
        const std::string &head_session_id,
        const req_body &body) {
    auto req_content = std::make_shared<dbc::query_node_rent_orders_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(QUERY_NODE_RENT_ORDERS_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::query_node_rent_orders_req_data req_data;
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    auto req_msg = std::make_shared<network::message>();
    req_msg->set_name(QUERY_NODE_RENT_ORDERS_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_query_node_rent_orders_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::query_node_rent_orders_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error2");
        LOG_ERROR << "rsq decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_query_node_rent_orders_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "query node rent orders timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// free memory
void rest_api_service::rest_free_memory(const std::shared_ptr<network::http_request>& httpReq, const std::string& path) {
	if (httpReq->get_request_method() != network::http_request::POST) {
		LOG_ERROR << "http request is not post";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
		return;
	}

	std::vector<std::string> path_list;
	util::split_path(path, path_list);
	if (path_list.size() != 1) {
		LOG_ERROR << "path_list's size != 2";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /mining_nodes/free_memory");
		return;
	}

	req_body body;
 
	std::string s_body = httpReq->read_body();
	if (s_body.empty()) {
		LOG_ERROR << "http request body is empty";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
	if (!ok) {
		std::stringstream ss;
		ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
		LOG_ERROR << ss.str();
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
		return;
	}

	if (!doc.IsObject()) {
		LOG_ERROR << "invalid json";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
		return;
	}

	std::string strerror;
	if (!parse_req_params(doc, body, strerror)) {
		LOG_ERROR << "parse req params failed: " << strerror;
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
		return;
	}

	if (!has_peer_nodeid(body)) {
		LOG_ERROR << "peer_nodeid is empty";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
		return;
	}

	if (body.session_id.empty() || body.session_id_sign.empty()) {
		if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
			LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
			return;
		}
	}

	// node request
	std::string head_session_id = util::create_session_id();

	auto node_req_msg = create_node_free_memory_req_msg(head_session_id, body);
	if (nullptr == node_req_msg) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
		return;
	}

	// timer
	uint32_t timer_id = add_timer(NODE_FREE_MEMORY_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
		CALLBACK_1(rest_api_service::on_node_free_memory_timer, this));
	if (INVALID_TIMER_ID == timer_id) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
		return;
	}

	// session
	if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
		remove_timer(timer_id);
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
		return;
	}

	// broadcast message
	if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
		return;
	}
}

std::shared_ptr<network::message> rest_api_service::create_node_free_memory_req_msg(const std::string& head_session_id,
    const req_body& body) {
	// 创建 node_ 请求
	std::shared_ptr<dbc::node_free_memory_req> req_content = std::make_shared<dbc::node_free_memory_req>();
	// header
	req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
	req_content->header.__set_msg_name(NODE_FREE_MEMORY_REQ);
	req_content->header.__set_nonce(util::create_nonce());
	req_content->header.__set_session_id(head_session_id);
	std::map<std::string, std::string> exten_info;
	exten_info["pub_key"] = body.pub_key;
	req_content->header.__set_exten_info(exten_info);
	std::vector<std::string> path;
	path.push_back(ConfManager::instance().GetNodeId());
	req_content->header.__set_path(path);

	// body
	dbc::node_free_memory_req_data req_data;
	req_data.__set_peer_nodes_list(body.peer_nodes_list);
	req_data.__set_additional(body.additional);
	req_data.__set_wallet(body.wallet);
	req_data.__set_nonce(body.nonce);
	req_data.__set_sign(body.sign);
	req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
	req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
	std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
	for (auto& it : body.multisig_accounts.signs) {
		dbc::multisig_sign_item item;
		item.wallet = it.wallet;
		item.nonce = it.nonce;
		item.sign = it.sign;
		vecMultisigSignItem.push_back(item);
	}
	req_data.__set_multisig_signs(vecMultisigSignItem);
	req_data.__set_session_id(body.session_id);
	req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

	// encrypt
	std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
	network::binary_protocol proto(out_buf.get());
	req_data.write(&proto);

	auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
	if (service_info != nullptr) {
		std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
		std::string priv_key = ConfManager::instance().GetPrivKey();

		if (!pub_key.empty() && !priv_key.empty()) {
			std::string s_data = encrypt_data((unsigned char*)out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
			req_content->body.__set_data(s_data);
		}
		else {
			LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
			return nullptr;
		}
	}
	else {
		LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
		return nullptr;
	}

	std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
	req_msg->set_name(NODE_FREE_MEMORY_REQ);
	req_msg->set_content(req_content);

	return req_msg;
}

void rest_api_service::on_node_free_memory_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
    const std::shared_ptr<network::message>& rsp_msg) {
	auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_free_memory_rsp>(rsp_msg->content);
	if (!node_rsp_msg) {
		LOG_ERROR << "node_rsp_msg is nullptr";
		return;
	}

	const std::shared_ptr<network::http_request>& httpReq = hreq_context->m_hreq;

	// decrypt
	std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
	std::string priv_key = ConfManager::instance().GetPrivKey();
	if (pub_key.empty() || priv_key.empty()) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
		LOG_ERROR << "pub_key or priv_key is empty";
		return;
	}

	std::string ori_message;
	try {
		bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
		if (!succ || ori_message.empty()) {
			httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error1");
			LOG_ERROR << "rsp decrypt error1";
			return;
		}
	}
	catch (std::exception& e) {
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsp decrypt error2");
		LOG_ERROR << "rsp decrypt error2";
		return;
	}

	rapidjson::Document doc;
	rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
	if (!ok) {
		LOG_ERROR << "http response parse error: " << rapidjson::GetParseError_En(ok.Code());
		httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "http response parse error");
		return;
	}

	httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_free_memory_timer(const std::shared_ptr<core_timer>& timer) {
	if (nullptr == timer) {
		LOG_ERROR << "timer is nullptr";
		return;
	}

	const std::string& session_id = timer->get_session_id();
	std::shared_ptr<service_session> session = get_session(session_id);
	if (nullptr == session) {
		LOG_ERROR << "session is nullptr";
		return;
	}

	variables_map& vm = session->get_context().get_args();
	if (0 == vm.count(HTTP_REQUEST_KEY)) {
		LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
		session->clear();
		this->remove_session(session_id);
		return;
	}

	auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
	if (nullptr != hreq_context) {
		hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "free memory timeout");
	}

	session->clear();
	this->remove_session(session_id);
}


// node session_id
void rest_api_service::rest_node_session_id(const std::shared_ptr<network::http_request> &httpReq,
                                            const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 1) {
        LOG_ERROR << "path_list's size != 1";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /mining_nodes/session_id");
        return;
    }

    req_body body;

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
        LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
        return;
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_session_id_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_SESSION_ID_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_session_id_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_session_id_req_msg(const std::string &head_session_id,
                                                                                        const req_body &body) {
    // 创建 node_ 请求
    std::shared_ptr<dbc::node_session_id_req> req_content = std::make_shared<dbc::node_session_id_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_SESSION_ID_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_session_id_req_data req_data;
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_SESSION_ID_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_session_id_rsp(const std::shared_ptr<network::http_request_context> &hreq_context,
                                              const std::shared_ptr<network::message> &rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_session_id_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "rsp is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error2");
        LOG_ERROR << "rsq decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_session_id_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "get session_id timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// /peers/
void rest_api_service::rest_peers(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.size() == 1) {
        const std::string &sOption = path_list[0];
        rest_get_peer_nodes(httpReq, path);
        return;
    }

    httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "invalid requests uri");
}

void reply_peer_nodes_list(const std::list<rsp_peer_node_info> &peer_nodes_list, std::string &data_json) {
    std::stringstream ss;
    ss << "{";
    ss << "\"errcode\":0";
    ss << ", \"message\":{";
    ss << "\"peer_nodes\":[";
    int peers_count = 0;
    for (auto& iter : peer_nodes_list) {
        if (peers_count > 0) ss << ",";
        ss << "{";
        ss << "\"node_id\":" << "\"" << iter.peer_node_id << "\"";
        ss << ", \"addr\":" << "\"" << iter.addr.get_ip() << ":" << iter.addr.get_port() << "\"";
        ss << "}";

        peers_count++;
    }
    ss << "]}";
    ss << "}";
    data_json = ss.str();
}

void reply_peer_nodes_list(const std::map<std::string, lan_machine_info> &lan_nodes, std::string &data_json) {
    std::stringstream ss;
    ss << "{";
    ss << "\"errcode\":0";
    ss << ", \"message\":{";
    ss << "\"peer_nodes\":[";
    int peers_count = 0;
    for (auto& iter : lan_nodes) {
        if (peers_count > 50) break;
        if (peers_count > 0) ss << ",";
        ss << "{";
        ss << "\"node_id\":" << "\"" << iter.first << "\"";
        ss << ", \"addr\":" << "\"" << iter.second.local_address << ":" << iter.second.local_port << "\"";
        switch (iter.second.node_type) {
            case NODE_TYPE::ComputeNode:
                ss << ", \"node_type\":" << "\"" << "computer" << "\"";
                break;
            case NODE_TYPE::ClientNode:
                ss << ", \"node_type\":" << "\"" << "client" << "\"";
                break;
            case NODE_TYPE::SeedNode:
                ss << ", \"node_type\":" << "\"" << "seed" << "\"";
                break;
            default:
                break;
        }
        ss << ", \"net_type\":" << "\"" << iter.second.net_type << "\"";
        ss << "}";

        peers_count++;
    }
    ss << "]}";
    ss << "}";
    data_json = ss.str();
}

void rest_api_service::rest_get_peer_nodes(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 1) {
        LOG_ERROR << "path_list's size != 1";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /peers/active or /peers/global");
        return;
    }

    req_body body;
    body.option = path_list[0];
    if (body.option == "active") {
        body.flag = QUERY_PEERS_FLAG::Active;
        std::unordered_map<std::string, std::shared_ptr<peer_node>> peer_nodes_map =
                p2p_net_service::instance().get_peer_nodes_map();
        std::vector<std::shared_ptr<peer_node>> vPeers;
        for (auto it = peer_nodes_map.begin(); it != peer_nodes_map.end(); ++it) {
            if (it->second->m_connection_status == connection_status::CONNECTED)
                vPeers.push_back(it->second);
        }
        srand((unsigned int)time(0));
        random_shuffle(vPeers.begin(), vPeers.end());

        std::list<rsp_peer_node_info> peer_nodes_list;
        for (auto itn = vPeers.begin(); itn != vPeers.end(); ++itn) {
            if (ConfManager::instance().GetNodeId() == (*itn)->m_id ||
                SystemInfo::instance().GetPublicip() == (*itn)->m_peer_addr.get_ip())
                continue;

            rsp_peer_node_info node_info;
            node_info.peer_node_id = (*itn)->m_id;
            node_info.live_time_stamp = (*itn)->m_live_time;
            node_info.addr.set_ip((*itn)->m_peer_addr.get_ip());
            node_info.addr.set_port((*itn)->m_peer_addr.get_port());
            node_info.node_type = (int8_t)(*itn)->m_node_type;
            node_info.service_list.clear();
            node_info.service_list.push_back(std::string("ai_training"));
            peer_nodes_list.push_back(std::move(node_info));

            if (peer_nodes_list.size() >= 50) break;
        }

        std::string data_json;
        reply_peer_nodes_list(peer_nodes_list, data_json);
        httpReq->reply_comm_rest_succ2(data_json);
    } else if (body.option == "global") {
        body.flag = QUERY_PEERS_FLAG::Global;
        std::list<std::shared_ptr<peer_candidate>> peer_candidates =
                p2p_net_service::instance().get_peer_candidates();
        std::vector<std::shared_ptr<peer_candidate>> vPeers;
        for (auto it = peer_candidates.begin(); it != peer_candidates.end(); ++it) {
            if ((*it)->net_st != net_state::ns_failed && (*it)->net_st != net_state::ns_zombie)
                vPeers.push_back(*it);
        }
        srand((unsigned int)time(0));
        random_shuffle(vPeers.begin(), vPeers.end());

        std::list<rsp_peer_node_info> peer_nodes_list;
        for (auto it = vPeers.begin(); it != vPeers.end(); ++it) {
            if (ConfManager::instance().GetNodeId() == (*it)->node_id ||
                SystemInfo::instance().GetPublicip() == (*it)->tcp_ep.address().to_string())
                continue;

            rsp_peer_node_info node_info;
            node_info.peer_node_id = (*it)->node_id;
            node_info.live_time_stamp = 0;
            node_info.net_st = (int8_t)(*it)->net_st;
            node_info.addr.set_ip((*it)->tcp_ep.address().to_string());
            node_info.addr.set_port((*it)->tcp_ep.port());
            node_info.node_type = (int8_t)(*it)->node_type;
            node_info.service_list.clear();
            node_info.service_list.push_back(std::string("ai_training"));
            peer_nodes_list.push_back(std::move(node_info));

            if (peer_nodes_list.size() >= 50) break;
        }

        std::string data_json;
        reply_peer_nodes_list(peer_nodes_list, data_json);
        httpReq->reply_comm_rest_succ2(data_json);
    } else if (body.option == "lan") {
        // body.flag = flag_active;
        const std::map<std::string, lan_machine_info>& lan_nodes =
            p2p_lan_service::instance().get_lan_nodes();
        std::string data_json;
        reply_peer_nodes_list(lan_nodes, data_json);
        httpReq->reply_comm_rest_succ2(data_json);
    } else {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid option. the option is active or global");
    }
}

// /stat/
void rest_api_service::rest_stat(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    /*
    rapidjson::Document document;
    rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

    rapidjson::Value data(rapidjson::kObjectType);
    std::string node_id = ConfManager::instance().GetNodeId();

    data.AddMember("node_id", STRING_REF(node_id), allocator);
    data.AddMember("session_count", rest_api_service::instance().get_session_count(), allocator);
    data.AddMember("startup_time", rest_api_service::instance().get_startup_time(), allocator);
    SUCC_REPLY(data)
    return nullptr;
    */
}

// /config/
void rest_api_service::rest_config(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    /*
    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.empty()) {

        std::string body = httpReq->read_body();
        if (body.empty()) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid body. Use /api/v1/config")
            return nullptr;
        }

        rapidjson::Document document;
        try {
            document.Parse(body.c_str());
            if (!document.IsObject()) {
                ERROR_REPLY(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "Invalid JSON. Use /api/v1/config")
                return nullptr;
            }
        } catch (...) {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_MISC_ERROR, "Parse JSON Error. Use /api/v1/config")
            return nullptr;
        }

        std::string log_level;


        JSON_PARSE_STRING(document, "log_level", log_level)

        std::map<std::string, uint32_t> log_level_to_int = {

                {"trace",   0},
                {"debug",   1},
                {"info",    2},
                {"warning", 3},
                {"error",   4},
                {"fatal",   5}
        };

        if (log_level_to_int.count(log_level)) {

            LOG_ERROR << "set log level " << log_level;
            uint32_t log_level_int = log_level_to_int[log_level];
            log::set_filter_level((boost::log::trivial::severity_level)
            log_level_int);
        } else {
            ERROR_REPLY(HTTP_BADREQUEST, RPC_MISC_ERROR, "illegal log level value")
            return nullptr;
        }
    }


    rapidjson::Document document;
    rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

    rapidjson::Value data(rapidjson::kObjectType);

    data.AddMember("result", "ok", allocator);
    SUCC_REPLY(data)

    return nullptr;
    */
}

void rest_api_service::on_binary_forward(const std::shared_ptr<network::message> &msg) {
    /*
    if (!msg) {
        LOG_ERROR << "recv logs_resp but msg is nullptr";
        return E_DEFAULT;
    }


    std::shared_ptr<network::msg_forward> content = std::dynamic_pointer_cast<network::msg_forward>(
            msg->content);

    if (!content) {
        LOG_ERROR << "not a valid binary forward msg";
        return E_DEFAULT;
    }

    // support request message name end with "_req" postfix
    auto &msg_name = msg->content->header.msg_name;

    if (msg_name.substr(msg_name.size() - 4) == std::string("_req")) {
        // add path
        msg->content->header.path.push_back(ConfManager::instance().GetNodeId());

        LOG_INFO << "broadcast_message binary forward msg";
        network::connection_manager::instance().broadcast_message(msg);
    } else {
        network::connection_manager::instance().send_resp_message(msg);
    }

    return ERR_SUCCESS;
    */
}

// /snapshot
void rest_api_service::rest_snapshot(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.size() >= 1 && path_list.size() <= 3) {
        std::string second_param = path_list[0];
        if (second_param == "create") {
            rest_create_snapshot(httpReq, path);
            return;
        } else if (second_param == "delete") {
            rest_delete_snapshot(httpReq, path);
            return;
        } else if (second_param == "list") {
            rest_list_snapshot(httpReq, path);
            return;
        }
    }

    httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "invalid uri");
}

// list snapshot
void rest_api_service::rest_list_snapshot(const std::shared_ptr<network::http_request> &httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

	std::vector<std::string> path_list;
	util::split_path(path, path_list);
	if (path_list.size() < 2) {
		LOG_ERROR << "path_list's size < 2";
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /snapshot/list/<task_id>[/snapshot_name]");
		return;
	}

    req_body body;
    
    if (path_list.size() >= 2) {
        body.task_id = path_list[1];
    }
    if (path_list.size() >= 3) {
        body.snapshot_name = path_list[2];
    }
 
    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
            return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_list_snapshot_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_LIST_SNAPSHOT_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_list_snapshot_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_list_snapshot_req_msg(const std::string &head_session_id,
                                                                                       const req_body& body) {
    auto req_content = std::make_shared<dbc::node_list_snapshot_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_LIST_SNAPSHOT_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_list_snapshot_req_data req_data;
    req_data.__set_task_id(body.task_id);
    req_data.__set_snapshot_name(body.snapshot_name);
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_LIST_SNAPSHOT_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_list_snapshot_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
                                             const std::shared_ptr<network::message>& rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_list_snapshot_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_list_snapshot_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "list snapshot timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// create snapshot
void rest_api_service::rest_create_snapshot(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 2) {
        LOG_ERROR << "path_list's size != 1";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use: /snapshot/<task_id>/create");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    req_body body;

    body.task_id = path_list[1];
    if (body.task_id.empty()) {
        LOG_ERROR << "task_id is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "task_id is empty");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

	const rapidjson::Value& v_additional = doc["additional"];
	if (!v_additional.IsObject()) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse request params failed: " + strerror);
		return;
	}
	// image_server
	std::string image_server;
	JSON_PARSE_STRING(v_additional, "image_server", image_server)

	auto it_svr = ConfManager::instance().FindImageServer(image_server);
	if (image_server.empty() || it_svr == nullptr) {
		httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "image_server not exist");
		return;
	}

	body.image_server = it_svr->to_string();

    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
            return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_create_snapshot_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_CREATE_SNAPSHOT_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_create_snapshot_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_create_snapshot_req_msg(const std::string &head_session_id, const req_body& body) {
    auto req_content = std::make_shared<dbc::node_create_snapshot_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_CREATE_SNAPSHOT_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_create_snapshot_req_data req_data;
    req_data.__set_task_id(body.task_id);
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_image_server(body.image_server);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_CREATE_SNAPSHOT_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_create_snapshot_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
                                                  const std::shared_ptr<network::message>& rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_create_snapshot_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_create_snapshot_timer(const std::shared_ptr<core_timer>& timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "create snapshot timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// delete snapshot
void rest_api_service::rest_delete_snapshot(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 3) {
        LOG_ERROR << "path_list's size != 2";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /snapshot/<task_id>/delete/<snapshot_name>");
        return;
    }

    req_body body;

    body.task_id = path_list[1];
    if (body.task_id.empty()) {
        LOG_ERROR << "task_id is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "task_id is empty");
        return;
    }

    body.snapshot_name = path_list[2];
    if (body.snapshot_name.empty()) {
        LOG_ERROR << "snapshot_name is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "snapshot_name is empty");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
            return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_delete_snapshot_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_DELETE_SNAPSHOT_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_delete_snapshot_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_delete_snapshot_req_msg(const std::string &head_session_id,
                                                                                         const req_body& body) {
    // 创建 node_ 请求
    std::shared_ptr<dbc::node_delete_snapshot_req> req_content = std::make_shared<dbc::node_delete_snapshot_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_DELETE_SNAPSHOT_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_delete_snapshot_req_data req_data;
    req_data.__set_task_id(body.task_id);
    req_data.__set_snapshot_name(body.snapshot_name);
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_DELETE_SNAPSHOT_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_delete_snapshot_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
                                                  const std::shared_ptr<network::message>& rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_delete_snapshot_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "rsp is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_delete_snapshot_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "delete snapshot timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// monitor
void rest_api_service::rest_monitor(const network::HTTP_REQUEST_PTR &httpReq, const std::string &path) {
    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.empty()) {
        rest_list_monitor_server(httpReq, path);
        return;
    }

    if (path_list.size() == 1) {
        const std::string &first_param = path_list[0];
        // create
        if (first_param == "set") {
            rest_set_monitor_server(httpReq, path);
        } else {
            rest_list_monitor_server(httpReq, path);
        }
        return;
    }

    httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "invalid requests uri");
}

void rest_api_service::rest_list_monitor_server(const std::shared_ptr<network::http_request> &httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    req_body body;

    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    // if (!path_list.empty()) {
    //     LOG_ERROR << "invalid uri path " << path;
    //     httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /monitor");
    //     return;
    // }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
            return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_list_monitor_server_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_LIST_MONITOR_SERVER_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_list_monitor_server_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_list_monitor_server_req_msg(const std::string &head_session_id,
                                                                                       const req_body& body) {
    auto req_content = std::make_shared<dbc::node_list_monitor_server_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_LIST_MONITOR_SERVER_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_list_monitor_server_req_data req_data;
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_LIST_MONITOR_SERVER_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_list_monitor_server_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
                                             const std::shared_ptr<network::message>& rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_list_monitor_server_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_list_monitor_server_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "list monitor server timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// set monitor server
void rest_api_service::rest_set_monitor_server(const std::shared_ptr<network::http_request>& httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 1) {
        LOG_ERROR << "path_list's size != 1";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use: /monitor/set");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    req_body body;

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
            return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_set_monitor_server_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_SET_MONITOR_SERVER_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_set_monitor_server_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_set_monitor_server_req_msg(const std::string &head_session_id, const req_body& body) {
    auto req_content = std::make_shared<dbc::node_set_monitor_server_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_SET_MONITOR_SERVER_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_set_monitor_server_req_data req_data;
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_SET_MONITOR_SERVER_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_set_monitor_server_rsp(const std::shared_ptr<network::http_request_context>& hreq_context,
                                                  const std::shared_ptr<network::message>& rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_set_monitor_server_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_set_monitor_server_timer(const std::shared_ptr<core_timer>& timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "set monitor server timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// local area netwrok
void rest_api_service::rest_lan(const network::HTTP_REQUEST_PTR &httpReq, const std::string &path) {
    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.empty()) {
        rest_list_lan(httpReq, path);
        return;
    }

    if (path_list.size() == 1) {
        const std::string &first_param = path_list[0];
        // create
        if (first_param == "create") {
            rest_create_lan(httpReq, path);
        } else {
            rest_list_lan(httpReq, path);
        }
        return;
    }

    if (path_list.size() == 2) {
        const std::string &first_param = path_list[0];
        if (first_param == "delete") {
            rest_delete_lan(httpReq, path);
        }
        return;
    }

    httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "invalid requests uri");
}

void vxlan_network_list(std::shared_ptr<dbc::networkInfo> network, std::string &data_json) {
    std::stringstream ss;
    ss << "{";
    ss << "\"errcode\":0";
    ss << ", \"message\":{";
    ss << "\"network_name\":" << "\"" << network->networkId << "\"";
    ss << ",\"bridge_name\":" << "\"" << network->bridgeName << "\"";
    ss << ",\"vxlan_name\":" << "\"" << network->vxlanName << "\"";
    ss << ",\"vxlan_vni\":" << "\"" << network->vxlanVni << "\"";
    ss << ",\"ip_cidr\":" << "\"" << network->ipCidr << "\"";
    ss << ",\"ip_start\":" << "\"" << network->ipStart << "\"";
    ss << ",\"ip_end\":" << "\"" << network->ipEnd << "\"";
    ss << ",\"dhcp_interface\":" << "\"" << network->dhcpInterface << "\"";
    ss << ",\"machine_id\":" << "\"" << network->machineId << "\"";
    ss << ",\"rent_wallet\":" << "\"" << network->rentWallet << "\"";
    ss << ",\"members\":" << "[";
    int memberCounts = 0;
    for (const auto& task_id : network->members) {
        if (memberCounts > 0) ss << ",";
        ss << "\"" << task_id << "\"";
        memberCounts++;
    }
    ss << "]";
    struct tm _tm{};
    time_t tt = network->lastUseTime;
    localtime_r(&tt, &_tm);
    char buf[256] = {0};
    memset(buf, 0, sizeof(char) * 256);
    strftime(buf, sizeof(char) * 256, "%Y-%m-%d %H:%M:%S", &_tm);
    ss << ",\"lastUseTime\":" << "\"" << buf << "\"";
    ss << ",\"native_flags\":" << "\"" << network->nativeFlags << "\"";
    ss << "}}";
    data_json = ss.str();
}

void vxlan_network_list(const std::map<std::string, std::shared_ptr<dbc::networkInfo>> &networks, std::string &data_json) {
    std::stringstream ss;
    ss << "{";
    ss << "\"errcode\":0";
    ss << ", \"message\":{";
    ss << "\"networks\":[";
    int count = 0;
    for (auto &it : networks) {
        if (count > 50) break;
        if (count > 0) ss << ",";
        ss << "{";
        ss << "\"network_name\":" << "\"" << it.second->networkId << "\"";
        ss << ",\"bridge_name\":" << "\"" << it.second->bridgeName << "\"";
        ss << ",\"vxlan_name\":" << "\"" << it.second->vxlanName << "\"";
        ss << ",\"vxlan_vni\":" << "\"" << it.second->vxlanVni << "\"";
        ss << ",\"ip_cidr\":" << "\"" << it.second->ipCidr << "\"";
        ss << ",\"ip_start\":" << "\"" << it.second->ipStart << "\"";
        ss << ",\"ip_end\":" << "\"" << it.second->ipEnd << "\"";
        ss << ",\"dhcp_interface\":" << "\"" << it.second->dhcpInterface << "\"";
        ss << ",\"machine_id\":" << "\"" << it.second->machineId << "\"";
        ss << ",\"rent_wallet\":" << "\"" << it.second->rentWallet << "\"";
        ss << ",\"members\":" << "[";
        int memberCounts = 0;
        for (const auto& task_id : it.second->members) {
            if (memberCounts > 0) ss << ",";
            ss << "\"" << task_id << "\"";
            memberCounts++;
        }
        ss << "]";
        struct tm _tm{};
        time_t tt = it.second->lastUseTime;
        localtime_r(&tt, &_tm);
        char buf[256] = {0};
        memset(buf, 0, sizeof(char) * 256);
        strftime(buf, sizeof(char) * 256, "%Y-%m-%d %H:%M:%S", &_tm);
        ss << ",\"lastUseTime\":" << "\"" << buf << "\"";
        ss << ",\"native_flags\":" << "\"" << it.second->nativeFlags << "\"";
        ss << "}";

        count++;
    }
    ss << "]}";
    ss << "}";

    data_json = ss.str();
}

// list local area network
void rest_api_service::rest_list_lan(const std::shared_ptr<network::http_request> &httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    req_body body;

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.empty()) {
        body.network_name = "";
    } else if (path_list.size() == 1) {
        body.network_name = path_list[0];
    } else {
        LOG_ERROR << "path_list's size > 1";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /lan");
        return;
    }

    // parse body
    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse req params failed: " + strerror);
        return;
    }

    // all peer_nodes
    if (body.peer_nodes_list.empty()) {
        std::string data_json;
        if (body.network_name.empty()) {
            const std::map<std::string, std::shared_ptr<dbc::networkInfo>> networks = VxlanManager::instance().GetNetworks();
            vxlan_network_list(networks, data_json);
        } else {
            std::shared_ptr<dbc::networkInfo> network = VxlanManager::instance().GetNetwork(body.network_name);
            if (network) {
                vxlan_network_list(network, data_json);
            } else {
                httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "network name not exist");
                return;
            }
        }
        httpReq->reply_comm_rest_succ2(data_json);
    } else {
		// node request
        std::string head_session_id = util::create_session_id();

        auto node_req_msg = create_node_list_lan_req_msg(head_session_id, body);
        if (nullptr == node_req_msg) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
            return;
        }

        // timer
        uint32_t timer_id = add_timer(NODE_LIST_LAN_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
            CALLBACK_1(rest_api_service::on_node_list_lan_timer, this));
        if (INVALID_TIMER_ID == timer_id) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
            return;
        }

        // session
        if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
            remove_timer(timer_id);
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
            return;
        }

        // broadcast message
        if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
            return;
        }
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_list_lan_req_msg(const std::string &head_session_id,
                                                                            const req_body &body) {
    auto req_content = std::make_shared<dbc::node_list_lan_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_LIST_LAN_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_list_lan_req_data req_data;
    req_data.__set_network_id(body.network_name);
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_LIST_LAN_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_list_lan_rsp(const std::shared_ptr<network::http_request_context> &hreq_context,
                                const std::shared_ptr<network::message> &rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_list_lan_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_list_lan_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "list local area network timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// create local area network
void rest_api_service::rest_create_lan(const std::shared_ptr<network::http_request> &httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 1) {
        LOG_ERROR << "path_list's size != 1";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use: /lan/create");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    req_body body;

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    // all peer_nodes
    // if (body.peer_nodes_list.empty()) {
    //     std::string networkName, vxlanVni, ipCidr;
    //     FResult fret = {ERR_ERROR, "invalid additional param"};
    //     if (doc.HasMember("additional") && doc["additional"].IsObject()) {
    //         const rapidjson::Value& obj_add = doc["additional"];
    //         if (obj_add.HasMember("network_name") && obj_add["network_name"].IsString())
    //             networkName = obj_add["network_name"].GetString();
    //         // if (obj_add.HasMember("vxlan_vni") && obj_add["vxlan_vni"].IsString())
    //         //     vxlanVni = obj_add["vxlan_vni"].GetString();
    //         if (obj_add.HasMember("ip_cidr") && obj_add["ip_cidr"].IsString())
    //             ipCidr = obj_add["ip_cidr"].GetString();
    //         fret = VxlanManager::instance().CreateNetworkServer(networkName, ipCidr);
    //     }
    //     std::stringstream ss;
    //     ss << "{";
    //     ss << "\"errcode\":" << (fret.errcode == ERR_SUCCESS ? 0 : fret.errcode);
    //     ss << ", \"message\":\"" << (fret.errcode == ERR_SUCCESS ? "ok" : fret.errmsg);
    //     ss << "\"}";
    //     httpReq->reply_comm_rest_succ2(ss.str());
    //     return;
    // }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
            return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_create_lan_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_CREATE_LAN_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_create_lan_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_create_lan_req_msg(const std::string &head_session_id,
                                                                            const req_body &body) {
    auto req_content = std::make_shared<dbc::node_create_lan_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_CREATE_LAN_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_create_lan_req_data req_data;
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_CREATE_LAN_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_create_lan_rsp(const std::shared_ptr<network::http_request_context> &hreq_context,
                                    const std::shared_ptr<network::message> &rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_create_lan_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_create_lan_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "create local area network timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// delete local area network
void rest_api_service::rest_delete_lan(const std::shared_ptr<network::http_request> &httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 2) {
        LOG_ERROR << "path_list's size != 2";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use: /lan/delete/<network_name>");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    req_body body;
    body.network_name = path_list[1];

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    // all peer_nodes
    if (body.peer_nodes_list.empty()) {
        // FResult fret = VxlanManager::instance().DeleteNetwork(body.network_name);
        // if (fret.errcode == ERR_SUCCESS)
        //     httpReq->reply_comm_rest_succ2("{\"errcode\": 0,\"message\": \"ok\"}");
        // else
        //     httpReq->reply_comm_rest_succ2("{\"errcode\": -1,\"message\": \"" + fret.errmsg + "\"}");
        // return;
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    } else {
		// node request
        std::string head_session_id = util::create_session_id();

        auto node_req_msg = create_node_delete_lan_req_msg(head_session_id, body);
        if (nullptr == node_req_msg) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
            return;
        }

        // timer
        uint32_t timer_id = add_timer(NODE_DELETE_LAN_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
            CALLBACK_1(rest_api_service::on_node_delete_lan_timer, this));
        if (INVALID_TIMER_ID == timer_id) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
            return;
        }

        // session
        if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
            remove_timer(timer_id);
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
            return;
        }

        // broadcast message
        if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
            return;
        }
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_delete_lan_req_msg(const std::string &head_session_id,
                                                                            const req_body &body) {
    auto req_content = std::make_shared<dbc::node_delete_lan_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_DELETE_LAN_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_delete_lan_req_data req_data;
    req_data.__set_network_id(body.network_name);
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_DELETE_LAN_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_delete_lan_rsp(const std::shared_ptr<network::http_request_context> &hreq_context,
                                    const std::shared_ptr<network::message> &rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_delete_lan_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_delete_lan_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "delete local area network timeout");
    }

    session->clear();
    this->remove_session(session_id);
}

// bare metal server
void rest_api_service::rest_bare_metal(const network::HTTP_REQUEST_PTR &httpReq, const std::string &path) {
    std::vector<std::string> path_list;
    util::split_path(path, path_list);

    if (path_list.empty()) {
        rest_list_bare_metal(httpReq, path);
        return;
    }

    if (path_list.size() == 1) {
        const std::string &first_param = path_list[0];
        // add
        if (first_param == "add") {
            rest_add_bare_metal(httpReq, path);
        } else if (first_param == "delete") {
            rest_delete_bare_metal(httpReq, path);
        } else if (first_param == "power") {
            rest_bare_metal_power(httpReq, path);
        } else if (first_param == "bootdev") {
            rest_bare_metal_bootdev(httpReq, path);
        } else {
            rest_list_bare_metal(httpReq, path);
        }
        return;
    }

    httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "invalid requests uri");
}

void bare_metal_list(const std::map<std::string, std::shared_ptr<dbc::db_bare_metal>> &nodes, std::string &data_json) {
    std::stringstream ss;
    ss << "{";
    ss << "\"errcode\":0";
    ss << ", \"message\":{";
    ss << "\"bare_metal_nodes\":[";
    int count = 0;
    for (auto &it : nodes) {
        if (count > 50) break;
        if (count > 0) ss << ",";
        ss << "{";
        ss << "\"node_id\":" << "\"" << it.second->node_id << "\"";
        ss << ",\"node_private_key\":" << "\"" << it.second->node_private_key << "\"";
        ss << ",\"uuid\":" << "\"" << it.second->uuid << "\"";
        ss << ",\"ip\":" << "\"" << it.second->ip << "\"";
        ss << ",\"os\":" << "\"" << it.second->os << "\"";
        ss << ",\"description\":" << "\"" << it.second->desc << "\"";
        ss << ",\"ipmi_hostname\":" << "\"" << it.second->ipmi_hostname << "\"";
        ss << ",\"ipmi_username\":" << "\"" << it.second->ipmi_username << "\"";
        ss << ",\"ipmi_password\":" << "\"" << it.second->ipmi_password << "\"";
        ss << "}";

        count++;
    }
    ss << "]}";
    ss << "}";

    data_json = ss.str();
}

// list bare metal server
void rest_api_service::rest_list_bare_metal(const std::shared_ptr<network::http_request> &httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    req_body body;

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.empty()) {
        body.node_id = "";
    } else if (path_list.size() == 1) {
        body.node_id = path_list[0];
    } else {
        LOG_ERROR << "path_list's size > 1";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use /bare_metal");
        return;
    }

    // parse body
    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse req params failed: " + strerror);
        return;
    }

    // all peer_nodes
    if (body.peer_nodes_list.empty()) {
        if (Server::NodeType != NODE_TYPE::BareMetalNode) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only bare metal node support");
            return;
        }

        std::string data_json;
        std::map<std::string, std::shared_ptr<dbc::db_bare_metal>> bare_metal_nodes;
        const std::map<std::string, std::shared_ptr<dbc::db_bare_metal>> origin_nodes = BareMetalNodeManager::instance().getBareMetalNodes();
        if (body.node_id.empty()) {
            bare_metal_nodes.insert(origin_nodes.begin(), origin_nodes.end());
        } else {
            for (auto& iter : origin_nodes) {
                if (iter.first == body.node_id || iter.second->uuid == body.node_id)
                    bare_metal_nodes.insert({iter.first, iter.second});
            }
        }
        bare_metal_list(bare_metal_nodes, data_json);
        httpReq->reply_comm_rest_succ2(data_json);
    } else {
        // node request
        std::string head_session_id = util::create_session_id();

        auto node_req_msg = create_node_list_bare_metal_req_msg(head_session_id, body);
        if (nullptr == node_req_msg) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
            return;
        }

        // timer
        uint32_t timer_id = add_timer(NODE_LIST_BARE_METAL_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
            CALLBACK_1(rest_api_service::on_node_list_bare_metal_timer, this));
        if (INVALID_TIMER_ID == timer_id) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
            return;
        }

        // session
        if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
            remove_timer(timer_id);
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
            return;
        }

        // broadcast message
        if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
            return;
        }
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_list_bare_metal_req_msg(
        const std::string &head_session_id, const req_body &body) {
    auto req_content = std::make_shared<dbc::node_list_bare_metal_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_LIST_BARE_METAL_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_list_bare_metal_req_data req_data;
    req_data.__set_node_id(body.node_id);
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_LIST_BARE_METAL_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_list_bare_metal_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_list_bare_metal_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_list_bare_metal_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "list bare metal timeout");
    }

    session->clear();
    this->remove_session(session_id);
}


// add bare metal server
void rest_api_service::rest_add_bare_metal(const std::shared_ptr<network::http_request> &httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 1) {
        LOG_ERROR << "path_list's size != 1";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use: /bare_metal/add");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    req_body body;

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (body.peer_nodes_list.empty()) {
        if (Server::NodeType != NODE_TYPE::BareMetalNode) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only bare metal node support");
            return;
        }

        const rapidjson::Value& v_additional = doc["additional"];
        if (!v_additional.IsObject()) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid additional param");
			return;
		}

        if (!v_additional.HasMember("bare_metal_nodes")) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse json additional bare_metal_nodes failed");
			return;
        }

        const rapidjson::Value& v_bm_nodes = v_additional["bare_metal_nodes"];
        if (!v_bm_nodes.IsArray()) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "additional bare_metal_nodes must be an array");
			return;
        }

        std::map<std::string, bare_metal_info> bare_metal_infos;
        std::map<std::string, std::string> ids;
        for (const auto& v : v_bm_nodes.GetArray()) {
            bare_metal_info info;
            if (v.HasMember("uuid") && v["uuid"].IsString())
                info.uuid = v["uuid"].GetString();
            if (v.HasMember("ip") && v["ip"].IsString())
                info.ip = v["ip"].GetString();
            if (v.HasMember("os") && v["os"].IsString())
                info.os = v["os"].GetString();
            if (v.HasMember("desc") && v["desc"].IsString())
                info.desc = v["desc"].GetString();
            if (v.HasMember("ipmi_hostname") && v["ipmi_hostname"].IsString())
                info.ipmi_hostname = v["ipmi_hostname"].GetString();
            if (v.HasMember("ipmi_username") && v["ipmi_username"].IsString())
                info.ipmi_username = v["ipmi_username"].GetString();
            if (v.HasMember("ipmi_password") && v["ipmi_password"].IsString())
                info.ipmi_password = v["ipmi_password"].GetString();
            if (!info.validate()) {
                httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "bare metal node info " + info.uuid + " invalid");
			    return;
            }
            if (BareMetalNodeManager::instance().ExistUUID(info.uuid)) {
                httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "bare metal node uuid " + info.uuid + " already existed");
			    return;
            }
            bare_metal_infos[info.uuid] = info;
        }

        if (bare_metal_infos.empty()) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "additional bare_metal_nodes is empty");
			return;
        }

        FResult fret = BareMetalNodeManager::instance().AddBareMetalNodes(std::move(bare_metal_infos), ids);
        std::stringstream ss;
        ss << "{";
        if (fret.errcode != ERR_SUCCESS) {
            ss << "\"errcode\":" << fret.errcode;
            ss << ", \"message\":\"" << fret.errmsg << "\"";
        } else {
            ss << "\"errcode\":" << 0;
            ss << ", \"message\":{\"bare_metal_nodes\":[";
            int count = 0;
            for (auto &it : ids) {
                if (count > 100) break;
                if (count > 0) ss << ",";
                ss << "{";
                ss << "\"node_id\":" << "\"" << it.second << "\"";
                // ss << ",\"node_private_key\":" << "\"" << it.second->node_private_key << "\"";
                ss << ",\"uuid\":" << "\"" << it.first << "\"";
                ss << "}";

                count++;
            }
            ss << "]}";
        }
        ss << "}";
        httpReq->reply_comm_rest_succ2(ss.str());
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    // if (body.session_id.empty() || body.session_id_sign.empty()) {
    //     if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
    //         LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
    //         httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
    //         return;
    //     }
    // }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_add_bare_metal_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_ADD_BARE_METAL_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_add_bare_metal_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_add_bare_metal_req_msg(
        const std::string &head_session_id, const req_body &body) {
    auto req_content = std::make_shared<dbc::node_add_bare_metal_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_ADD_BARE_METAL_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_add_bare_metal_req_data req_data;
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_ADD_BARE_METAL_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_add_bare_metal_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_add_bare_metal_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_add_bare_metal_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "add bare metal timeout");
    }

    session->clear();
    this->remove_session(session_id);
}


// delete bare metal server
void rest_api_service::rest_delete_bare_metal(const std::shared_ptr<network::http_request> &httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 1) {
        LOG_ERROR << "path_list's size != 1";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use: /bare_metal/delete");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    req_body body;

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    // all peer_nodes
    if (body.peer_nodes_list.empty()) {
        if (Server::NodeType != NODE_TYPE::BareMetalNode) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only bare metal node support");
            return;
        }

        std::vector<std::string> ids;
        const rapidjson::Value& v_additional = doc["additional"];
		if (!v_additional.IsObject()) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse json additional failed");
			return;
		}

        if (!v_additional.HasMember("bare_metal_node_ids")) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse json additional bare_metal_node_ids failed");
			return;
        }

        const rapidjson::Value& v_bm_ids = v_additional["bare_metal_node_ids"];
        if (!v_bm_ids.IsArray()) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "additional bare_metal_node_ids must be an array");
			return;
        }

        for (const auto& v : v_bm_ids.GetArray()) {
            if (v.IsString()) {
                ids.push_back(v.GetString());
            } else {
                httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "bare_metal_node_ids item must be a string");
			    return;
            }
        }

        if (ids.empty()) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "additional bare_meta_node_ids is an empty array");			return;
        }

        FResult fret = BareMetalNodeManager::instance().DeleteBareMetalNode(ids);
        if (fret.errcode > 0)
            httpReq->reply_comm_rest_succ2("{\"errcode\": 0,\"message\": \"" + fret.errmsg + "\"}");
        else
            httpReq->reply_comm_rest_succ2("{\"errcode\": -1,\"message\": \"" + fret.errmsg + "\"}");
        return;
    } else {
        // node request
        std::string head_session_id = util::create_session_id();

        auto node_req_msg = create_node_delete_bare_metal_req_msg(head_session_id, body);
        if (nullptr == node_req_msg) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
            return;
        }

        // timer
        uint32_t timer_id = add_timer(NODE_DELETE_BARE_METAL_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
            CALLBACK_1(rest_api_service::on_node_delete_bare_metal_timer, this));
        if (INVALID_TIMER_ID == timer_id) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
            return;
        }

        // session
        if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
            remove_timer(timer_id);
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
            return;
        }

        // broadcast message
        if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
            return;
        }
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_delete_bare_metal_req_msg(
        const std::string &head_session_id, const req_body &body) {
    auto req_content = std::make_shared<dbc::node_delete_bare_metal_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_DELETE_BARE_METAL_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_delete_bare_metal_req_data req_data;
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_DELETE_BARE_METAL_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_delete_bare_metal_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_delete_bare_metal_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_delete_bare_metal_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "delete bare metal timeout");
    }

    session->clear();
    this->remove_session(session_id);
}


// bare metal power on/off/reset
void rest_api_service::rest_bare_metal_power(const std::shared_ptr<network::http_request> &httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 1) {
        LOG_ERROR << "path_list's size != 1";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use: /bare_metal/power");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    req_body body;

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
            return;
        }
    }

    // check power command
    {
        const rapidjson::Value& v_additional = doc["additional"];
		if (!v_additional.IsObject()) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse json additional failed");
			return;
		}

        if (!v_additional.HasMember("command")) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse json additional command failed");
			return;
        }

        const rapidjson::Value& v_command = v_additional["command"];
        if (!v_command.IsString()) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "additional command must be a string");
			return;
        }

        std::string command = v_command.GetString();
        if (command != "status" && command != "on" && command != "off" && command != "reset") {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid command");
			return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_bare_metal_power_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_BARE_METAL_POWER_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_bare_metal_power_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_bare_metal_power_req_msg(
        const std::string &head_session_id, const req_body &body) {
    auto req_content = std::make_shared<dbc::node_bare_metal_power_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_BARE_METAL_POWER_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_bare_metal_power_req_data req_data;
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_BARE_METAL_POWER_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_bare_metal_power_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_bare_metal_power_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_bare_metal_power_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "bare metal power timeout");
    }

    session->clear();
    this->remove_session(session_id);
}


// bare metal boot device order pxe/disk/cdrom
void rest_api_service::rest_bare_metal_bootdev(const std::shared_ptr<network::http_request> &httpReq, const std::string &path) {
    if (httpReq->get_request_method() != network::http_request::POST) {
        LOG_ERROR << "http request is not post";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "only support POST request");
        return;
    }

    std::vector<std::string> path_list;
    util::split_path(path, path_list);
    if (path_list.size() != 1) {
        LOG_ERROR << "path_list's size != 1";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid uri, please use: /bare_metal/bootdev");
        return;
    }

    std::string s_body = httpReq->read_body();
    if (s_body.empty()) {
        LOG_ERROR << "http request body is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "http request body is empty");
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(s_body.c_str());
    if (!ok) {
        std::stringstream ss;
        ss << "json parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        LOG_ERROR << ss.str();
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, ss.str());
        return;
    }

    if (!doc.IsObject()) {
        LOG_ERROR << "invalid json";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid json");
        return;
    }

    req_body body;

    std::string strerror;
    if (!parse_req_params(doc, body, strerror)) {
        LOG_ERROR << "parse req params failed: " << strerror;
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, strerror);
        return;
    }

    if (!has_peer_nodeid(body)) {
        LOG_ERROR << "peer_nodeid is empty";
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "peer_nodeid is empty");
        return;
    }

    if (body.session_id.empty() || body.session_id_sign.empty()) {
        if (!check_wallet_sign(body) && !check_multisig_wallets(body)) {
            LOG_ERROR << "wallet_sign and multisig_wallets all invalid";
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "wallet_sign and multisig_wallets all invalid");
            return;
        }
    }

    // check boot device
    {
        const rapidjson::Value& v_additional = doc["additional"];
		if (!v_additional.IsObject()) {
			httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse json additional failed");
			return;
		}

        if (!v_additional.HasMember("device")) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "parse json additional device failed");
			return;
        }

        const rapidjson::Value& v_device = v_additional["device"];
        if (!v_device.IsString()) {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "additional device must be a string");
			return;
        }

        std::string device = v_device.GetString();
        if (device != "none" && device != "pxe" && device != "cdrom" && device != "disk" && device != "bios") {
            httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_PARAMS, "invalid device");
			return;
        }
    }

    // node request
    std::string head_session_id = util::create_session_id();

    auto node_req_msg = create_node_bare_metal_bootdev_req_msg(head_session_id, body);
    if (nullptr == node_req_msg) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create node request failed");
        return;
    }

    // timer
    uint32_t timer_id = add_timer(NODE_BARE_METAL_BOOTDEV_TIMER, MAX_WAIT_HTTP_RESPONSE_TIME, MAX_WAIT_HTTP_RESPONSE_TIME, 1, head_session_id,
        CALLBACK_1(rest_api_service::on_node_bare_metal_bootdev_timer, this));
    if (INVALID_TIMER_ID == timer_id) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "add timer failed");
        return;
    }

    // session
    if (ERR_SUCCESS != create_request_session(timer_id, httpReq, node_req_msg, head_session_id, body.peer_nodes_list[0])) {
        remove_timer(timer_id);
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "create request session failed");
        return;
    }

    // broadcast message
    if (network::connection_manager::instance().broadcast_message(node_req_msg) != ERR_SUCCESS) {
        httpReq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_RESPONSE_ERROR, "broadcast request failed");
        return;
    }
}

std::shared_ptr<network::message> rest_api_service::create_node_bare_metal_bootdev_req_msg(
        const std::string &head_session_id, const req_body &body) {
    auto req_content = std::make_shared<dbc::node_bare_metal_bootdev_req>();
    // header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(NODE_BARE_METAL_BOOTDEV_REQ);
    req_content->header.__set_nonce(util::create_nonce());
    req_content->header.__set_session_id(head_session_id);
    std::map<std::string, std::string> exten_info;
    exten_info["pub_key"] = body.pub_key;
    req_content->header.__set_exten_info(exten_info);
    std::vector<std::string> path;
    path.push_back(ConfManager::instance().GetNodeId());
    req_content->header.__set_path(path);

    // body
    dbc::node_bare_metal_bootdev_req_data req_data;
    req_data.__set_peer_nodes_list(body.peer_nodes_list);
    req_data.__set_additional(body.additional);
    req_data.__set_wallet(body.wallet);
    req_data.__set_nonce(body.nonce);
    req_data.__set_sign(body.sign);
    req_data.__set_multisig_wallets(body.multisig_accounts.wallets);
    req_data.__set_multisig_threshold(body.multisig_accounts.threshold);
    std::vector<dbc::multisig_sign_item> vecMultisigSignItem;
    for (auto& it : body.multisig_accounts.signs) {
        dbc::multisig_sign_item item;
        item.wallet = it.wallet;
        item.nonce = it.nonce;
        item.sign = it.sign;
        vecMultisigSignItem.push_back(item);
    }
    req_data.__set_multisig_signs(vecMultisigSignItem);
    req_data.__set_session_id(body.session_id);
    req_data.__set_session_id_sign(body.session_id_sign);
    req_data.__set_rent_order(body.rent_order);

    // encrypt
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    req_data.write(&proto);

    auto service_info = ServiceInfoManager::instance().find(body.peer_nodes_list[0]);
    if (service_info != nullptr) {
        std::string pub_key = service_info->kvs.count("pub_key") ? service_info->kvs["pub_key"] : "";
        std::string priv_key = ConfManager::instance().GetPrivKey();

        if (!pub_key.empty() && !priv_key.empty()) {
            std::string s_data = encrypt_data((unsigned char*) out_buf->get_read_ptr(), out_buf->get_valid_read_len(), pub_key, priv_key);
            req_content->body.__set_data(s_data);
        } else {
            LOG_ERROR << "pub_key is empty, node_id:" << body.peer_nodes_list[0];
            return nullptr;
        }
    } else {
        LOG_ERROR << "ServiceInfoManager not found node_id:" << body.peer_nodes_list[0];
        return nullptr;
    }

    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    req_msg->set_name(NODE_BARE_METAL_BOOTDEV_REQ);
    req_msg->set_content(req_content);

    return req_msg;
}

void rest_api_service::on_node_bare_metal_bootdev_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg) {
    auto node_rsp_msg = std::dynamic_pointer_cast<dbc::node_bare_metal_bootdev_rsp>(rsp_msg->content);
    if (!node_rsp_msg) {
        LOG_ERROR << "node_rsp_msg is nullptr";
        return;
    }

    const std::shared_ptr<network::http_request> &httpReq = hreq_context->m_hreq;

    // decrypt
    std::string pub_key = node_rsp_msg->header.exten_info["pub_key"];
    std::string priv_key = ConfManager::instance().GetPrivKey();
    if (pub_key.empty() || priv_key.empty()) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "pub_key or priv_key is empty");
        LOG_ERROR << "pub_key or priv_key is empty";
        return;
    }

    std::string ori_message;
    try {
        bool succ = decrypt_data(node_rsp_msg->body.data, pub_key, priv_key, ori_message);
        if (!succ || ori_message.empty()) {
            httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "rsq decrypt error1");
            LOG_ERROR << "rsq decrypt error1";
            return;
        }
    } catch (std::exception &e) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "req decrypt error2");
        LOG_ERROR << "req decrypt error2";
        return;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ori_message.c_str());
    if (!ok) {
        httpReq->reply_comm_rest_err(HTTP_INTERNAL, -1, "response parse error");
        LOG_ERROR << "response parse error: " << rapidjson::GetParseError_En(ok.Code()) << "(" << ok.Offset() << ")";
        return;
    }

    httpReq->reply_comm_rest_succ2(ori_message);
}

void rest_api_service::on_node_bare_metal_bootdev_timer(const std::shared_ptr<core_timer> &timer) {
    if (nullptr == timer) {
        LOG_ERROR << "timer is nullptr";
        return;
    }

    const std::string &session_id = timer->get_session_id();
    std::shared_ptr<service_session> session = get_session(session_id);
    if (nullptr == session) {
        LOG_ERROR << "session is nullptr";
        return;
    }

    variables_map &vm = session->get_context().get_args();
    if (0 == vm.count(HTTP_REQUEST_KEY)) {
        LOG_ERROR << "session's context has no HTTP_REQUEST_KEY";
        session->clear();
        this->remove_session(session_id);
        return;
    }

    auto hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<network::http_request_context>>();
    if (nullptr != hreq_context) {
        hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, -1, "bare metal bootdev timeout");
    }

    session->clear();
    this->remove_session(session_id);
}
