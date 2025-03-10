#ifndef REST_API_SERVICE_H
#define REST_API_SERVICE_H

#include "message/matrix_types.h"
#include "network/http/http_server.h"
#include "service_module/service_module.h"
#include "timer/timer.h"
#include "util/utils.h"

static const std::string REST_API_URI = "/api/v1";

constexpr int MIN_INIT_HTTP_SERVER_TIME = 5000;     // ms
constexpr int MAX_WAIT_HTTP_RESPONSE_TIME = 30000;  // ms
constexpr int MAX_SESSION_COUNT = 1024;

struct req_sign_item {
    std::string wallet;
    std::string nonce;
    std::string sign;
};

struct req_multisig {
    std::vector<std::string> wallets;
    int32_t threshold;
    std::vector<req_sign_item> signs;
};

struct req_body {
    std::vector<std::string> peer_nodes_list;
    std::string additional;
    std::string wallet;
    std::string nonce;
    std::string sign;
    req_multisig multisig_accounts;
    std::string session_id;
    std::string session_id_sign;
    std::string vm_xml;
    std::string vm_xml_url;
    std::string pub_key;

    std::string task_id;
    std::string image_server;

    // restart force
    int16_t force_reboot = 0;

    // task logs
    QUERY_LOG_DIRECTION head_or_tail = QUERY_LOG_DIRECTION::Tail;
    int32_t number_of_lines = 100;

    // peers
    std::string option;
    QUERY_PEERS_FLAG flag = QUERY_PEERS_FLAG::Global;

    // snapshot
    std::string snapshot_name;

    // vxlan network id
    std::string network_name;

    // bare metal node id
    std::string node_id;

    // rent order id
    std::string rent_order;
};

class rsp_peer_node_info {
public:
    std::string peer_node_id;
    int32_t live_time_stamp;
    int8_t net_st = -1;
    network::net_address addr;
    int8_t node_type = 0;
    std::vector<std::string> service_list;
};

class rest_api_service : public service_module,
                         public network::http_request_event,
                         public Singleton<rest_api_service> {
public:
    rest_api_service() = default;

    ~rest_api_service() override = default;

    ERRCODE init() override;

    void exit() override;

    void on_http_request_event(
        std::shared_ptr<network::http_request> &hreq) override;

    // /
    void rest_client_version(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    // /tasks
    void rest_task(const std::shared_ptr<network::http_request> &httpReq,
                   const std::string &path);

    // list task
    void rest_list_task(const std::shared_ptr<network::http_request> &httpReq,
                        const std::string &path);

    std::shared_ptr<network::message> create_node_list_task_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_list_task_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_list_task_timer(const std::shared_ptr<core_timer> &timer);

    // create task
    void rest_create_task(const std::shared_ptr<network::http_request> &httpReq,
                          const std::string &path);

    std::shared_ptr<network::message> create_node_create_task_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_create_task_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_create_task_timer(const std::shared_ptr<core_timer> &timer);

    // start task
    void rest_start_task(const std::shared_ptr<network::http_request> &httpReq,
                         const std::string &path);

    std::shared_ptr<network::message> create_node_start_task_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_start_task_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_start_task_timer(const std::shared_ptr<core_timer> &timer);

    // shutdown task
    void rest_shutdown_task(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_shutdown_task_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_shutdown_task_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_shutdown_task_timer(const std::shared_ptr<core_timer> &timer);

    // poweroff task
    void rest_poweroff_task(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_poweroff_task_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_poweroff_task_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_poweroff_task_timer(const std::shared_ptr<core_timer> &timer);

    // stop task
    void rest_stop_task(const std::shared_ptr<network::http_request> &httpReq,
                        const std::string &path);

    std::shared_ptr<network::message> create_node_stop_task_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_stop_task_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_stop_task_timer(const std::shared_ptr<core_timer> &timer);

    // restart task
    void rest_restart_task(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_restart_task_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_restart_task_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_restart_task_timer(const std::shared_ptr<core_timer> &timer);

    // reset task
    void rest_reset_task(const std::shared_ptr<network::http_request> &httpReq,
                         const std::string &path);

    std::shared_ptr<network::message> create_node_reset_task_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_reset_task_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_reset_task_timer(const std::shared_ptr<core_timer> &timer);

    // delete task
    void rest_delete_task(const std::shared_ptr<network::http_request> &httpReq,
                          const std::string &path);

    std::shared_ptr<network::message> create_node_delete_task_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_delete_task_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_delete_task_timer(const std::shared_ptr<core_timer> &timer);

    // modify task
    void rest_modify_task(const std::shared_ptr<network::http_request> &httpReq,
                          const std::string &path);

    std::shared_ptr<network::message> create_node_modify_task_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_modify_task_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_modify_task_timer(const std::shared_ptr<core_timer> &timer);

    // set task user password
    void rest_passwd_task(const std::shared_ptr<network::http_request> &httpReq,
                          const std::string &path);

    std::shared_ptr<network::message> create_node_passwd_task_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_passwd_task_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_passwd_task_timer(const std::shared_ptr<core_timer> &timer);

    // task logs
    void rest_task_logs(const std::shared_ptr<network::http_request> &httpReq,
                        const std::string &path);

    std::shared_ptr<network::message> create_node_task_logs_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_task_logs_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_task_logs_timer(const std::shared_ptr<core_timer> &timer);

    // /images
    void rest_images(const std::shared_ptr<network::http_request> &httpReq,
                     const std::string &path);

    // list image_server
    void rest_list_image_servers(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_list_image_servers_req_msg(
        const std::string &head_session_id, const req_body &body);

    // list image
    void rest_list_images(const std::shared_ptr<network::http_request> &httpReq,
                          const std::string &path);

    std::shared_ptr<network::message> create_node_list_images_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_list_images_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_list_images_timer(const std::shared_ptr<core_timer> &timer);

    // download image
    void rest_download_image(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_download_image_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_download_image_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_download_image_timer(const std::shared_ptr<core_timer> &timer);

    // download progress
    void rest_download_image_progress(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message>
    create_node_download_image_progress_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_download_image_progress_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_download_image_progress_timer(
        const std::shared_ptr<core_timer> &timer);

    // stop download
    void rest_stop_download_image(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_stop_download_image_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_stop_download_image_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_stop_download_image_timer(
        const std::shared_ptr<core_timer> &timer);

    // upload image
    void rest_upload_image(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_upload_image_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_upload_image_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_upload_image_timer(const std::shared_ptr<core_timer> &timer);

    // upload progress
    void rest_upload_image_progress(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_upload_image_progress_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_upload_image_progress_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_upload_image_progress_timer(
        const std::shared_ptr<core_timer> &timer);

    // stop upload
    void rest_stop_upload_image(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_stop_upload_image_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_stop_upload_image_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_stop_upload_image_timer(
        const std::shared_ptr<core_timer> &timer);

    // delete image
    void rest_delete_image(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_delete_image_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_delete_image_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_delete_image_timer(const std::shared_ptr<core_timer> &timer);

    // /disk
    void rest_disk(const std::shared_ptr<network::http_request> &httpReq,
                   const std::string &path);

    // list disk
    void rest_list_disk(const std::shared_ptr<network::http_request> &httpReq,
                        const std::string &path);

    std::shared_ptr<network::message> create_node_list_disk_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_list_disk_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_list_disk_timer(const std::shared_ptr<core_timer> &timer);

    // resize disk
    void rest_resize_disk(const std::shared_ptr<network::http_request> &httpReq,
                          const std::string &path);

    std::shared_ptr<network::message> create_node_resize_disk_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_resize_disk_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_resize_disk_timer(const std::shared_ptr<core_timer> &timer);

    // add disk
    void rest_add_disk(const std::shared_ptr<network::http_request> &httpReq,
                       const std::string &path);

    std::shared_ptr<network::message> create_node_add_disk_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_add_disk_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_add_disk_timer(const std::shared_ptr<core_timer> &timer);

    // delete disk
    void rest_delete_disk(const std::shared_ptr<network::http_request> &httpReq,
                          const std::string &path);

    std::shared_ptr<network::message> create_node_delete_disk_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_delete_disk_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_delete_disk_timer(const std::shared_ptr<core_timer> &timer);

    // /mining_nodes
    void rest_mining_nodes(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    // list mining_node
    void rest_list_mining_nodes(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_query_node_info_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_query_node_info_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_query_node_info_timer(
        const std::shared_ptr<core_timer> &timer);

    // query node rent orders
    void rest_rent_orders(const std::shared_ptr<network::http_request> &httpReq,
                          const std::string &path);

    void rest_query_node_rent_orders(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_query_node_rent_orders_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_query_node_rent_orders_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_query_node_rent_orders_timer(
        const std::shared_ptr<core_timer> &timer);

    // free memory
    void rest_free_memory(const std::shared_ptr<network::http_request> &httpReq,
                          const std::string &path);

    std::shared_ptr<network::message> create_node_free_memory_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_free_memory_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_free_memory_timer(const std::shared_ptr<core_timer> &timer);

    // session_id
    void rest_node_session_id(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_session_id_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_session_id_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_session_id_timer(const std::shared_ptr<core_timer> &timer);

    // peers
    void rest_peers(const std::shared_ptr<network::http_request> &httpReq,
                    const std::string &path);

    void rest_get_peer_nodes(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    // /stat
    void rest_stat(const std::shared_ptr<network::http_request> &httpReq,
                   const std::string &path);

    // /config
    void rest_config(const std::shared_ptr<network::http_request> &httpReq,
                     const std::string &path);

    void on_binary_forward(const std::shared_ptr<network::message> &msg);

    // /snapshot
    void rest_snapshot(const std::shared_ptr<network::http_request> &httpReq,
                       const std::string &path);

    // list snapshot
    void rest_list_snapshot(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_list_snapshot_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_list_snapshot_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_list_snapshot_timer(const std::shared_ptr<core_timer> &timer);

    // create snapshot
    void rest_create_snapshot(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_create_snapshot_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_create_snapshot_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_create_snapshot_timer(
        const std::shared_ptr<core_timer> &timer);

    // delete snapshot
    void rest_delete_snapshot(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_delete_snapshot_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_delete_snapshot_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_delete_snapshot_timer(
        const std::shared_ptr<core_timer> &timer);

    // monitor
    void rest_monitor(const network::HTTP_REQUEST_PTR &httpReq,
                      const std::string &path);

    // list monitor server
    void rest_list_monitor_server(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_list_monitor_server_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_list_monitor_server_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_list_monitor_server_timer(
        const std::shared_ptr<core_timer> &timer);

    // set monitor server
    void rest_set_monitor_server(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_set_monitor_server_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_set_monitor_server_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_set_monitor_server_timer(
        const std::shared_ptr<core_timer> &timer);

    // local area netwrok
    void rest_lan(const network::HTTP_REQUEST_PTR &httpReq,
                  const std::string &path);

    // list local area network
    void rest_list_lan(const std::shared_ptr<network::http_request> &httpReq,
                       const std::string &path);

    std::shared_ptr<network::message> create_node_list_lan_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_list_lan_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_list_lan_timer(const std::shared_ptr<core_timer> &timer);

    // create local area network
    void rest_create_lan(const std::shared_ptr<network::http_request> &httpReq,
                         const std::string &path);

    std::shared_ptr<network::message> create_node_create_lan_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_create_lan_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_create_lan_timer(const std::shared_ptr<core_timer> &timer);

    // delete local area network
    void rest_delete_lan(const std::shared_ptr<network::http_request> &httpReq,
                         const std::string &path);

    std::shared_ptr<network::message> create_node_delete_lan_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_delete_lan_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_delete_lan_timer(const std::shared_ptr<core_timer> &timer);

    // bare metal server
    void rest_bare_metal(const network::HTTP_REQUEST_PTR &httpReq,
                         const std::string &path);

    // list bare metal server
    void rest_list_bare_metal(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_list_bare_metal_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_list_bare_metal_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_list_bare_metal_timer(
        const std::shared_ptr<core_timer> &timer);

    // add bare metal server
    void rest_add_bare_metal(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_add_bare_metal_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_add_bare_metal_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_add_bare_metal_timer(const std::shared_ptr<core_timer> &timer);

    // delete bare metal server
    void rest_delete_bare_metal(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_delete_bare_metal_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_delete_bare_metal_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_delete_bare_metal_timer(
        const std::shared_ptr<core_timer> &timer);

    // modify bare metal server
    void rest_modify_bare_metal(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_modify_bare_metal_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_modify_bare_metal_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_modify_bare_metal_timer(
        const std::shared_ptr<core_timer> &timer);

    // bare metal power on/off/reset
    void rest_bare_metal_power(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_bare_metal_power_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_bare_metal_power_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_bare_metal_power_timer(
        const std::shared_ptr<core_timer> &timer);

    // bare metal boot device order pxe/disk/cdrom
    void rest_bare_metal_bootdev(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_bare_metal_bootdev_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_bare_metal_bootdev_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_bare_metal_bootdev_timer(
        const std::shared_ptr<core_timer> &timer);

    // deeplink
    void rest_deeplink(const network::HTTP_REQUEST_PTR &httpReq,
                       const std::string &path);

    // list deeplink info
    void rest_list_deeplink_info(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_list_deeplink_info_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_list_deeplink_info_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_list_deeplink_info_timer(
        const std::shared_ptr<core_timer> &timer);

    // set deeplink info
    void rest_set_deeplink_info(
        const std::shared_ptr<network::http_request> &httpReq,
        const std::string &path);

    std::shared_ptr<network::message> create_node_set_deeplink_info_req_msg(
        const std::string &head_session_id, const req_body &body);

    void on_node_set_deeplink_info_rsp(
        const std::shared_ptr<network::http_request_context> &hreq_context,
        const std::shared_ptr<network::message> &rsp_msg);

    void on_node_set_deeplink_info_timer(
        const std::shared_ptr<core_timer> &timer);

protected:
    void init_timer() override;

    void init_invoker() override;

    int32_t create_request_session(
        uint32_t timer_id, const std::shared_ptr<network::http_request> &hreq,
        const std::shared_ptr<network::message> &req_msg,
        const std::string &session_id, const std::string &peer_node_id);

    void on_call_rsp_handler(const std::shared_ptr<network::message> &msg);

    bool check_rsp_header(const std::shared_ptr<network::message> &rsp_msg);

private:
    std::vector<network::http_path_handler> m_path_handlers;
    std::map<std::string, network::response_call_handler> m_rsp_handlers;
    bloomlru_filter m_nonce_filter{1000000};
};

#endif
