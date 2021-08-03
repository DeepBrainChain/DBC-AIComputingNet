#ifndef DBC_REST_HANDLER_H
#define DBC_REST_HANDLER_H

#include "util/utils.h"
#include "network/http_server.h"

static const std::string REST_API_VERSION = "v1.1";
static const std::string REST_API_URI = "/api/v1";

class RestHandler {
public:
    std::shared_ptr<dbc::network::message> rest_task(const dbc::network::HTTP_REQUEST_PTR &httpReq, const std::string &path);

    // create
    std::shared_ptr<dbc::network::message> rest_create_task(const dbc::network::HTTP_REQUEST_PTR &httpReq, const std::string &path);

    int32_t on_cmd_create_task_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg);

    // start <task_id>
    std::shared_ptr<dbc::network::message> rest_start_task(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_cmd_start_task_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg);

    // restart <task_id>
    std::shared_ptr<dbc::network::message> rest_restart_task(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_cmd_restart_task_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg);

    // stop <task_id>
    std::shared_ptr<dbc::network::message> rest_stop_task(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_cmd_stop_task_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg);

    // reset <task_id>
    std::shared_ptr<dbc::network::message> rest_reset_task(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_cmd_reset_task_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg);

    // delete <task_id>
    std::shared_ptr<dbc::network::message> rest_delete_task(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_cmd_delete_task_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg);

    // list
    std::shared_ptr<dbc::network::message> rest_list_task(const dbc::network::HTTP_REQUEST_PTR &httpReq, const std::string &path);

    int32_t on_cmd_list_task_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg);

    // modify <task_id>
    std::shared_ptr<dbc::network::message> rest_modify_task(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_cmd_modify_task_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg);

    // logs
    std::shared_ptr<dbc::network::message> rest_task_logs(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_cmd_task_logs_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg);

    // peers
    std::shared_ptr<dbc::network::message> rest_peers(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_cmd_get_peer_nodes_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg);

    // /stat/
    std::shared_ptr<dbc::network::message> rest_stat(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path);

    // /mining_nodes/
    std::shared_ptr<dbc::network::message> rest_mining_nodes(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path);

    int32_t on_list_node_rsp(const dbc::network::HTTP_REQ_CTX_PTR& hreq_context, std::shared_ptr<dbc::network::message> &resp_msg);

    // /config/
    std::shared_ptr<dbc::network::message> rest_config(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path);

    // /
    std::shared_ptr<dbc::network::message> rest_api_version(const dbc::network::HTTP_REQUEST_PTR& httpReq, const std::string &path);
};

#endif
