#ifndef REST_API_SERVICE_H
#define REST_API_SERVICE_H

#include "util/utils.h"
#include "service_module/service_module.h"
#include "network/http_server.h"
#include "timer/timer.h"
#include "service/message/cmd_message.h"

static const std::string REST_API_VERSION = "v1.1";
static const std::string REST_API_URI = "/api/v1";

constexpr int MIN_INIT_HTTP_SERVER_TIME = 5000;    //ms
constexpr int MAX_WAIT_HTTP_RESPONSE_TIME = 30000; //ms
constexpr int MAX_SESSION_COUNT = 1024;

struct sign_item {
    std::string wallet;
    std::string nonce;
    std::string sign;
};

struct multisig {
    std::vector<std::string> wallets;
    int32_t threshold;
    std::vector<sign_item> signs;
};


struct req_body {
    std::vector<std::string> peer_nodes_list;
    std::string additional;
    std::string wallet;
    std::string nonce;
    std::string sign;
    multisig multisig_accounts;
    std::string session_id;
    std::string session_id_sign;
    std::string vm_xml;
    std::string vm_xml_url;
    std::string pub_key;

    std::string task_id;

    // restart force
    int16_t force_reboot = 0;

    // task logs
    int16_t head_or_tail = GET_LOG_TAIL;
    int32_t number_of_lines = 100;

    // peers
    std::string option;
    int16_t flag = flag_global;

    // snapshot
    std::string snapshot_name;
};

class rest_api_service : public service_module,
        public dbc::network::http_request_event,
        public Singleton<rest_api_service> {
public:
    rest_api_service() = default;

    ~rest_api_service() override = default;

    int32_t init() override;

    void on_http_request_event(std::shared_ptr<dbc::network::http_request> &hreq) override;

    // /
    void rest_client_version(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path);

    // /images
    void rest_images(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path);

    // list image
    void rest_list_images(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path);

    std::shared_ptr<dbc::network::message> create_node_list_images_req_msg(const std::string &head_session_id,
                                                                           const req_body &body);

    void on_node_list_images_rsp(const std::shared_ptr<dbc::network::http_request_context> &hreq_context,
                                 const std::shared_ptr<dbc::network::message> &rsp_msg);

    void on_node_list_images_timer(const std::shared_ptr<core_timer> &timer);

    // download image
    void rest_download_image(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path);

    std::shared_ptr<dbc::network::message> create_node_download_image_req_msg(const std::string &head_session_id,
                                                                              const req_body &body);

    void on_node_download_image_rsp(const std::shared_ptr<dbc::network::http_request_context> &hreq_context,
                                    const std::shared_ptr<dbc::network::message> &rsp_msg);

    void on_node_download_image_timer(const std::shared_ptr<core_timer> &timer);

    // upload image
    void rest_upload_image(const std::shared_ptr<dbc::network::http_request>& httpReq, const std::string &path);

    std::shared_ptr<dbc::network::message> create_node_upload_image_req_msg(const std::string &head_session_id,
                                                                            const req_body &body);

    void on_node_upload_image_rsp(const std::shared_ptr<dbc::network::http_request_context> &hreq_context,
                                  const std::shared_ptr<dbc::network::message> &rsp_msg);

    void on_node_upload_image_timer(const std::shared_ptr<core_timer> &timer);

    // /tasks
    void rest_task(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    // list task
    void rest_list_task(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    std::shared_ptr<dbc::network::message> create_node_list_task_req_msg(const std::string &head_session_id,
                                                                         const req_body &body);

    void on_node_list_task_rsp(const std::shared_ptr<dbc::network::http_request_context> &hreq_context,
                               const std::shared_ptr<dbc::network::message> &rsp_msg);

    void on_node_list_task_timer(const std::shared_ptr<core_timer> &timer);

    // create task
    void rest_create_task(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    std::shared_ptr<dbc::network::message> create_node_create_task_req_msg(const std::string &head_session_id,
                                                                           const req_body &body);

    void on_node_create_task_rsp(const std::shared_ptr<dbc::network::http_request_context> &hreq_context,
                                 const std::shared_ptr<dbc::network::message> &rsp_msg);

    void on_node_create_task_timer(const std::shared_ptr<core_timer> &timer);

    // start task
    void rest_start_task(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    std::shared_ptr<dbc::network::message> create_node_start_task_req_msg(const std::string &head_session_id,
                                                                          const req_body &body);

    void on_node_start_task_rsp(const std::shared_ptr<dbc::network::http_request_context> &hreq_context,
                                const std::shared_ptr<dbc::network::message> &rsp_msg);

    void on_node_start_task_timer(const std::shared_ptr<core_timer> &timer);

    // stop task
    void rest_stop_task(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    std::shared_ptr<dbc::network::message> create_node_stop_task_req_msg(const std::string &head_session_id,
                                                                         const req_body &body);

    void on_node_stop_task_rsp(const std::shared_ptr<dbc::network::http_request_context> &hreq_context,
                               const std::shared_ptr<dbc::network::message> &rsp_msg);

    void on_node_stop_task_timer(const std::shared_ptr<core_timer> &timer);

    // restart task
    void rest_restart_task(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    std::shared_ptr<dbc::network::message> create_node_restart_task_req_msg(const std::string &head_session_id,
                                                                            const req_body &body);

    void on_node_restart_task_rsp(const std::shared_ptr<dbc::network::http_request_context> &hreq_context,
                                  const std::shared_ptr<dbc::network::message> &rsp_msg);

    void on_node_restart_task_timer(const std::shared_ptr<core_timer> &timer);

    // reset task
    void rest_reset_task(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    std::shared_ptr<dbc::network::message> create_node_reset_task_req_msg(const std::string &head_session_id,
                                                                          const req_body &body);

    void on_node_reset_task_rsp(const std::shared_ptr<dbc::network::http_request_context> &hreq_context,
                                const std::shared_ptr<dbc::network::message> &rsp_msg);

    void on_node_reset_task_timer(const std::shared_ptr<core_timer> &timer);

    // delete task
    void rest_delete_task(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    std::shared_ptr<dbc::network::message> create_node_delete_task_req_msg(const std::string &head_session_id,
                                                                           const req_body &body);

    void on_node_delete_task_rsp(const std::shared_ptr<dbc::network::http_request_context> &hreq_context,
                                 const std::shared_ptr<dbc::network::message> &rsp_msg);

    void on_node_delete_task_timer(const std::shared_ptr<core_timer> &timer);

    // modify task
    void rest_modify_task(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    std::shared_ptr<dbc::network::message> create_node_modify_task_req_msg(const std::string &head_session_id,
                                                                           const req_body &body);

    void on_node_modify_task_rsp(const std::shared_ptr<dbc::network::http_request_context> &hreq_context,
                                 const std::shared_ptr<dbc::network::message> &rsp_msg);

    void on_node_modify_task_timer(const std::shared_ptr<core_timer> &timer);

    // task logs
    void rest_task_logs(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    std::shared_ptr<dbc::network::message> create_node_task_logs_req_msg(const std::string &head_session_id,
                                                                         const req_body &body);

    void on_node_task_logs_rsp(const std::shared_ptr<dbc::network::http_request_context> &hreq_context,
                               const std::shared_ptr<dbc::network::message> &rsp_msg);

    void on_node_task_logs_timer(const std::shared_ptr<core_timer> &timer);

    // /mining_nodes
    void rest_mining_nodes(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    // list mining node
    void rest_list_mining_nodes(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    std::shared_ptr<dbc::network::message> create_node_query_node_info_req_msg(const std::string &head_session_id,
                                                                               const req_body &body);

    void on_node_query_node_info_rsp(const std::shared_ptr<dbc::network::http_request_context> &hreq_context,
                                     const std::shared_ptr<dbc::network::message> &rsp_msg);

    void on_node_query_node_info_timer(const std::shared_ptr<core_timer> &timer);

    // mining node session_id
    void rest_node_session_id(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    std::shared_ptr<dbc::network::message> create_node_session_id_req_msg(const std::string &head_session_id,
                                                                          const req_body &body);

    void on_node_session_id_rsp(const std::shared_ptr<dbc::network::http_request_context> &hreq_context,
                                const std::shared_ptr<dbc::network::message> &rsp_msg);

    void on_node_session_id_timer(const std::shared_ptr<core_timer> &timer);

    // peers
    void rest_peers(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    void rest_get_peer_nodes(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    // /stat/
    void rest_stat(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    // /config/
    void rest_config(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    void on_binary_forward(const std::shared_ptr<dbc::network::message> &msg);

    // snapshot
    void rest_snapshot(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    // list snapshot
    void rest_list_snapshot(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    std::shared_ptr<dbc::network::message> create_node_list_snapshot_req_msg(const std::string &head_session_id,
                                                                             const req_body &body);

    void on_node_list_snapshot_rsp(const std::shared_ptr<dbc::network::http_request_context> &hreq_context,
                                   const std::shared_ptr<dbc::network::message> &rsp_msg);

    void on_node_list_snapshot_timer(const std::shared_ptr<core_timer> &timer);

    // create snapshot
    void rest_create_snapshot(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    std::shared_ptr<dbc::network::message> create_node_create_snapshot_req_msg(const std::string &head_session_id,
                                                                               const req_body &body);

    void on_node_create_snapshot_rsp(const std::shared_ptr<dbc::network::http_request_context> &hreq_context,
                                     const std::shared_ptr<dbc::network::message> &rsp_msg);

    void on_node_create_snapshot_timer(const std::shared_ptr<core_timer> &timer);

    // delete snapshot
    void rest_delete_snapshot(const std::shared_ptr<dbc::network::http_request> &httpReq, const std::string &path);

    std::shared_ptr<dbc::network::message> create_node_delete_snapshot_req_msg(const std::string &head_session_id,
                                                                               const req_body &body);

    void on_node_delete_snapshot_rsp(const std::shared_ptr<dbc::network::http_request_context> &hreq_context,
                                     const std::shared_ptr<dbc::network::message> &rsp_msg);

    void on_node_delete_snapshot_timer(const std::shared_ptr<core_timer> &timer);

protected:
    void init_timer() override;

    void init_invoker() override;

    void init_subscription() override;

    int32_t create_request_session(const std::string &timer_id, const std::shared_ptr<dbc::network::http_request> &hreq,
                                   const std::shared_ptr<dbc::network::message> &req_msg, const std::string &session_id,
                                   const std::string &peer_node_id);

    void on_call_rsp_handler(const std::shared_ptr<dbc::network::message> &msg);

    bool check_rsp_header(const std::shared_ptr<dbc::network::message> &rsp_msg);

private:
    std::vector<dbc::network::http_path_handler> m_path_handlers;
    std::map<std::string, dbc::network::response_call_handler> m_rsp_handlers;
    lru::Cache<std::string, int32_t, std::mutex> m_nonceCache{1000000, 0};
};

#endif
