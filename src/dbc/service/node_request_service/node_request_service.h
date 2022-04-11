#ifndef DBC_NODE_REQUEST_SERVICE_H
#define DBC_NODE_REQUEST_SERVICE_H

#include "util/utils.h"
#include <leveldb/db.h>
#include "service_module/service_module.h"
#include <boost/process.hpp>
#include <boost/asio.hpp>
#include "task/TaskManager.h"
#include "util/bloomlru_filter.h"
#include "message/matrix_types.h"
#include "message/lan_types.h"
#include "service_info/ServiceInfoManager.h"
#include "util/system_info.h"
#include "network/protocol/protocol.h"

namespace bp = boost::process;
using namespace boost::asio::ip;

struct AuthorityParams {
    std::string wallet;
    std::string nonce;
    std::string sign;
    std::vector<std::string> multisig_wallets;
    int32_t multisig_threshold;
    std::vector<dbc::multisig_sign_item> multisig_signs;
    std::string session_id;
    std::string session_id_sign;
};

class node_request_service : public service_module, public Singleton<node_request_service> {
public:
    node_request_service() = default;

    ~node_request_service() override = default;

    ERRCODE init() override;

    void exit() override;

protected:
    void add_self_to_servicelist();

    void init_timer() override;

    void init_invoker() override;

    void on_node_list_images_req(const std::shared_ptr<network::message> &msg);

    void list_images(const network::base_header& header, const std::shared_ptr<dbc::node_list_images_req_data>& data, const AuthoriseResult& result);

    void on_node_download_image_req(const std::shared_ptr<network::message> &msg);

    void download_image(const network::base_header& header, const std::shared_ptr<dbc::node_download_image_req_data>& data, const AuthoriseResult& result);

    void on_node_download_image_progress_req(const std::shared_ptr<network::message>& msg);

    void download_image_progress(const network::base_header& header, const std::shared_ptr<dbc::node_download_image_progress_req_data>& data, const AuthoriseResult& result);

    void on_node_stop_download_image_req(const std::shared_ptr<network::message>& msg);

    void stop_download_image(const network::base_header& header, const std::shared_ptr<dbc::node_stop_download_image_req_data>& data, const AuthoriseResult& result);

    void on_node_upload_image_req(const std::shared_ptr<network::message> &msg);

    void upload_image(const network::base_header& header, const std::shared_ptr<dbc::node_upload_image_req_data>& data, const AuthoriseResult& result);

    void on_node_upload_image_progress_req(const std::shared_ptr<network::message>& msg);

    void upload_image_progress(const network::base_header& header, const std::shared_ptr<dbc::node_upload_image_progress_req_data>& data, const AuthoriseResult& result);

    void on_node_stop_upload_image_req(const std::shared_ptr<network::message>& msg);

    void stop_upload_image(const network::base_header& header, const std::shared_ptr<dbc::node_stop_upload_image_req_data>& data, const AuthoriseResult& result);

    void on_node_delete_image_req(const std::shared_ptr<network::message>& msg);

    void delete_image(const network::base_header& header, const std::shared_ptr<dbc::node_delete_image_req_data>& data, const AuthoriseResult& result);

    void on_node_query_node_info_req(const std::shared_ptr<network::message> &msg);

    void query_node_info(const network::base_header& header, const std::shared_ptr<dbc::node_query_node_info_req_data>& data);

    void on_node_list_task_req(const std::shared_ptr<network::message>& msg);

    void task_list(const network::base_header& header, const std::shared_ptr<dbc::node_list_task_req_data>& data, const AuthoriseResult& result);

    void on_node_create_task_req(const std::shared_ptr<network::message>& msg);

    void task_create(const network::base_header& header, const std::shared_ptr<dbc::node_create_task_req_data>& data, const AuthoriseResult& result);

    void on_node_start_task_req(const std::shared_ptr<network::message>& msg);

    void task_start(const network::base_header& header, const std::shared_ptr<dbc::node_start_task_req_data>& data, const AuthoriseResult& result);

    void on_node_restart_task_req(const std::shared_ptr<network::message>& msg);

    void task_restart(const network::base_header& header, const std::shared_ptr<dbc::node_restart_task_req_data>& data, const AuthoriseResult& result);

    void on_node_stop_task_req(const std::shared_ptr<network::message>& msg);

    void task_stop(const network::base_header& header, const std::shared_ptr<dbc::node_stop_task_req_data>& data, const AuthoriseResult& result);

    void on_node_reset_task_req(const std::shared_ptr<network::message>& msg);

    void task_reset(const network::base_header& header, const std::shared_ptr<dbc::node_reset_task_req_data>& data, const AuthoriseResult& result);

    void on_node_delete_task_req(const std::shared_ptr<network::message>& msg);

    void task_delete(const network::base_header& header, const std::shared_ptr<dbc::node_delete_task_req_data>& data, const AuthoriseResult& result);

    void on_node_task_logs_req(const std::shared_ptr<network::message>& msg);

    void task_logs(const network::base_header& header, const std::shared_ptr<dbc::node_task_logs_req_data>& data, const AuthoriseResult& result);

    void on_node_modify_task_req(const std::shared_ptr<network::message>& msg);

    void task_modify(const network::base_header& header, const std::shared_ptr<dbc::node_modify_task_req_data>& data, const AuthoriseResult& result);

    void on_node_session_id_req(const std::shared_ptr<network::message>& msg);

    void node_session_id(const network::base_header& header, const std::shared_ptr<dbc::node_session_id_req_data>& data, const AuthoriseResult& result);

    void on_training_task_timer(const std::shared_ptr<core_timer>& timer);

    void on_prune_task_timer(const std::shared_ptr<core_timer>& timer);

    void on_timer_service_broadcast(const std::shared_ptr<core_timer>& timer);

    std::shared_ptr<network::message> create_service_broadcast_req_msg(const std::map <std::string, std::shared_ptr<dbc::node_service_info>>& mp);

    void on_net_service_broadcast_req(const std::shared_ptr<network::message> &msg);

    std::string format_logs(const std::string& raw_logs, uint16_t max_lines);

    void on_node_list_snapshot_req(const std::shared_ptr<network::message>& msg);

    void snapshot_list(const network::base_header& header, const std::shared_ptr<dbc::node_list_snapshot_req_data>& data, const AuthoriseResult& result);

    void on_node_create_snapshot_req(const std::shared_ptr<network::message>& msg);

    void snapshot_create(const network::base_header& header, const std::shared_ptr<dbc::node_create_snapshot_req_data>& data, const AuthoriseResult& result);

    void on_node_delete_snapshot_req(const std::shared_ptr<network::message>& msg);

    void snapshot_delete(const network::base_header& header, const std::shared_ptr<dbc::node_delete_snapshot_req_data>& data, const AuthoriseResult& result);

    void on_node_list_monitor_server_req(const std::shared_ptr<network::message>& msg);

    void monitor_server_list(const network::base_header& header, const std::shared_ptr<dbc::node_list_monitor_server_req_data>& data, const AuthoriseResult& result);

    void on_node_set_monitor_server_req(const std::shared_ptr<network::message>& msg);

    void monitor_server_set(const network::base_header& header, const std::shared_ptr<dbc::node_set_monitor_server_req_data>& data, const AuthoriseResult& result);
    
    void on_node_list_lan_req(const std::shared_ptr<network::message>& msg);

    void list_lan(const network::base_header& header, const std::shared_ptr<dbc::node_list_lan_req_data>& data, const AuthoriseResult& result);

    void on_node_create_lan_req(const std::shared_ptr<network::message>& msg);

    void create_lan(const network::base_header& header, const std::shared_ptr<dbc::node_create_lan_req_data>& data, const AuthoriseResult& result);
    
    void on_node_delete_lan_req(const std::shared_ptr<network::message>& msg);

    void delete_lan(const network::base_header& header, const std::shared_ptr<dbc::node_delete_lan_req_data>& data, const AuthoriseResult& result);

private:
    bool check_req_header(const std::shared_ptr<network::message> &msg);

    bool check_req_header_nonce(const std::string& nonce);

    bool hit_node(const std::vector<std::string>& peer_node_list, const std::string& node_id);

    FResult check_nonce(const std::string& wallet, const std::string& nonce, const std::string& sign);

    FResult check_nonce(const std::vector<std::string>& multisig_wallets, int32_t multisig_threshold,
                        const std::vector<dbc::multisig_sign_item>& multisig_signs);

    std::tuple<std::string, std::string> parse_wallet(const AuthorityParams& params);

    void check_authority(const AuthorityParams& params, AuthoriseResult& result);

protected:
    TaskMgr m_task_scheduler;
    bloomlru_filter m_nonce_filter{ 1000000 };
};

#endif
