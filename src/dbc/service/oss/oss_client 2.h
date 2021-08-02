#ifndef DBC_OSS_CLIENT_H
#define DBC_OSS_CLIENT_H

#include "util/utils.h"
#include "network/http_client.h"
#include "bill/bill_message.h"
#include "idle_task/idle_task_message.h"

class oss_client
{
public:
    oss_client(const std::string & url, const std::string &crt);

    //auth_task
    std::shared_ptr<auth_task_resp> post_auth_task(std::shared_ptr<auth_task_req> req);
    int32_t  post_auth_stop_task(std::shared_ptr<auth_task_req> req);
    int32_t post_auth_restart_task(std::shared_ptr<auth_task_req> req);
    int32_t post_auth_create_task(std::shared_ptr<auth_task_req> req);
    int32_t post_auth_update_task(std::shared_ptr<auth_task_req> req);
    //idle_task
    std::shared_ptr<idle_task_resp> post_idle_task(std::shared_ptr<idle_task_req> req);

    ~oss_client() = default;
protected:

    dbc::network::http_client m_http_client;

    std::string m_url;
    std::string m_crt;
};

#endif
