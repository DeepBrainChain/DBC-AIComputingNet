#include "rest_handler.h"
#include "../peer_request_service/peer_candidate.h"
#include "log/log.h"
#include "server/server.h"
#include "rest_api_service.h"
#include "../message/cmd_message.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"
#include "rapidjson/error/error.h"

/*
 * Unified successful response method
 * */
#define ERROR_REPLY(status, error_code, error_message) httpReq->reply_comm_rest_err(status,error_code,error_message);

/*
 * Unified error response method
 * */
#define SUCC_REPLY(data) httpReq->reply_comm_rest_succ(data);

/*
 * Initialize the context information that handles a response
 * */
#define INIT_RSP_CONTEXT(CMD_REQ, CMD_RSP) \
std::shared_ptr<dbc::network::http_request>& httpReq=hreq_context->m_hreq;\
std::shared_ptr<dbc::network::message>& req_msg=hreq_context->m_req_msg;\
std::shared_ptr<CMD_REQ> req = std::dynamic_pointer_cast<CMD_REQ>(req_msg->content);\
std::shared_ptr<CMD_RSP> resp = std::dynamic_pointer_cast<CMD_RSP>(resp_msg->content);\
rapidjson::Document document;\
rapidjson::Document::AllocatorType& allocator = document.GetAllocator();\
rapidjson::Value data(rapidjson::kObjectType);

static void fill_json_filed_string(rapidjson::Value &data, rapidjson::Document::AllocatorType &allocator,
                                   const std::string& key, const std::string &val) {
    if (val.empty()) return;

    std::string tmp_val(val);
    tmp_val = util::rtrim(tmp_val, '\n');
    util::trim(tmp_val);

    data.AddMember(STRING_DUP(key), STRING_DUP(tmp_val), allocator);
}




