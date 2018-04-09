/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºhttp_client.cpp
* description    £ºhttp client for rpc
* date                  : 2018.04.07
* author            £ºBruce Feng
**********************************************************************************/

#include "http_client.h"
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>


namespace matrix
{
    namespace core
    {

        const char *http_errorstring(int code)
        {
            switch (code) 
            {
            case EVREQ_HTTP_TIMEOUT:
                return "timeout reached";
            case EVREQ_HTTP_EOF:
                return "EOF reached";
            case EVREQ_HTTP_INVALID_HEADER:
                return "error while reading header, or invalid header";
            case EVREQ_HTTP_BUFFER_ERROR:
                return "error encountered while reading or writing";
            case EVREQ_HTTP_REQUEST_CANCEL:
                return "request was canceled";
            case EVREQ_HTTP_DATA_TOO_LONG:
                return "resp body is larger than allowed";
            default:
                return "unknown";
            }
        }

        static void http_request_done(struct evhttp_request *req, void *ctx)
        {
            http_reply *reply = static_cast<http_reply*>(ctx);

            if (req == nullptr) 
            {
                /* If req is nullptr, it means an error occurred while connecting: the
                * error code will have been passed to http_error_cb.
                */
                reply->status = 0;
                return;
            }

            reply->status = evhttp_request_get_response_code(req);

            struct evbuffer *buf = evhttp_request_get_input_buffer(req);
            if (buf)
            {
                size_t size = evbuffer_get_length(buf);
                const char *data = (const char*)evbuffer_pullup(buf, size);
                if (data)
                    reply->body = std::string(data, size);
                evbuffer_drain(buf, size);
            }
        }

        static void http_error_cb(enum evhttp_request_error err, void *ctx)
        {
            http_reply *reply = static_cast<http_reply*>(ctx);
            reply->error = err;
        }

        http_client::http_client(std::string remote_ip, uint16_t remote_port)
            : m_remote_ip(remote_ip)
            , m_remote_port(remote_port)
        {

        }

        void http_client::set_remote(std::string remote_ip, uint16_t remote_port)
        {
            m_remote_ip = remote_ip;
            m_remote_port = remote_port;
        }

        int32_t http_client::post(const std::string &endpoint, const kvs &headers, rapidjson::StringBuffer & req_content, http_reply &resp)
        {
            //event base
            raii_event_base base = obtain_event_base();

            //connection base
            raii_evhttp_connection evcon = obtain_evhttp_connection_base(base.get(), m_remote_ip, m_remote_port);
            evhttp_connection_set_timeout(evcon.get(), DEFAULT_HTTP_TIME_OUT);

            //http request
            raii_evhttp_request req = obtain_evhttp_request(http_request_done, (void*)&resp);
            if (nullptr == req)
            {
                return E_DEFAULT;
            }

            evhttp_request_set_error_cb(req.get(), http_error_cb);

            struct evkeyvalq* output_headers = evhttp_request_get_output_headers(req.get());
            assert(output_headers);

            //http header
            for (auto it = headers.begin(); it != headers.end(); it++)
            {
                evhttp_add_header(output_headers, it->first.c_str(), it->second.c_str());
            }

            // request data
            std::string strRequest = req_content.GetString() + std::string("\n");
            struct evbuffer* output_buffer = evhttp_request_get_output_buffer(req.get());
            assert(output_buffer);
            evbuffer_add(output_buffer, strRequest.data(), strRequest.size());

            //make request
            int r = evhttp_make_request(evcon.get(), req.get(), EVHTTP_REQ_POST, endpoint.c_str());
            req.release();                 // ownership moved to evcon in above call
            if (0 != r)
            {
                return E_DEFAULT;
            }

            event_base_dispatch(base.get());

            if (resp.status == 0)
            {
                LOG_ERROR << "http client could not connect to server: " << http_errorstring(resp.error);
                return E_DEFAULT;
            }
            else if (resp.status == 401)
            {
                LOG_ERROR << "http client authorization failed";
                return E_DEFAULT;
            }
            else if (resp.status >= 400 && resp.status != HTTP_BADREQUEST && resp.status != HTTP_NOTFOUND && resp.status != HTTP_INTERNAL)
            {
                LOG_ERROR << "http client error:  server returned HTTP error " << resp.status;
                return E_DEFAULT;
            }
            else if (resp.body.empty())
            {
                LOG_ERROR << "http client error:  no resp from server";
                return E_DEFAULT;
            }            

            return E_SUCCESS;
        }

        int32_t http_client::get(const std::string &endpoint, const kvs &headers, http_reply &resp)
        {
            //event base
            raii_event_base base = obtain_event_base();

            //connection base
            raii_evhttp_connection evcon = obtain_evhttp_connection_base(base.get(), m_remote_ip, m_remote_port);
            evhttp_connection_set_timeout(evcon.get(), DEFAULT_HTTP_TIME_OUT);

            //http request
            raii_evhttp_request req = obtain_evhttp_request(http_request_done, (void*)&resp);
            if (nullptr == req)
            {
                return E_DEFAULT;
            }

            evhttp_request_set_error_cb(req.get(), http_error_cb);

            struct evkeyvalq* output_headers = evhttp_request_get_output_headers(req.get());
            assert(output_headers);

            //http header
            for (auto it = headers.begin(); it != headers.end(); it++)
            {
                evhttp_add_header(output_headers, it->first.c_str(), it->second.c_str());
            }

            //make request
            int r = evhttp_make_request(evcon.get(), req.get(), EVHTTP_REQ_GET, endpoint.c_str());
            req.release();                 // ownership moved to evcon in above call
            if (0 != r)
            {
                return E_DEFAULT;
            }

            event_base_dispatch(base.get());

            if (resp.status == 0)
            {
                LOG_ERROR << "http client could not connect to server: " << http_errorstring(resp.error);
                return E_DEFAULT;
            }
            else if (resp.status == 401)
            {
                LOG_ERROR << "http client authorization failed";
                return E_DEFAULT;
            }
            else if (resp.status >= 400 && resp.status != HTTP_BADREQUEST && resp.status != HTTP_NOTFOUND && resp.status != HTTP_INTERNAL)
            {
                LOG_ERROR << "http client error:  server returned HTTP error " << resp.status;
                return E_DEFAULT;
            }
            else if (resp.body.empty())
            {
                LOG_ERROR << "http client error:  no resp from server";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t http_client::del(const std::string &endpoint, const kvs &headers, http_reply &resp)
        {
            //event base
            raii_event_base base = obtain_event_base();

            //connection base
            raii_evhttp_connection evcon = obtain_evhttp_connection_base(base.get(), m_remote_ip, m_remote_port);
            evhttp_connection_set_timeout(evcon.get(), DEFAULT_HTTP_TIME_OUT);

            //http request
            raii_evhttp_request req = obtain_evhttp_request(http_request_done, (void*)&resp);
            if (nullptr == req)
            {
                return E_DEFAULT;
            }

            evhttp_request_set_error_cb(req.get(), http_error_cb);

            struct evkeyvalq* output_headers = evhttp_request_get_output_headers(req.get());
            assert(output_headers);

            //http header
            for (auto it = headers.begin(); it != headers.end(); it++)
            {
                evhttp_add_header(output_headers, it->first.c_str(), it->second.c_str());
            }

            //make request
            int r = evhttp_make_request(evcon.get(), req.get(), EVHTTP_REQ_DELETE, endpoint.c_str());
            req.release();                 // ownership moved to evcon in above call
            if (0 != r)
            {
                return E_DEFAULT;
            }

            event_base_dispatch(base.get());

            if (resp.status == 0)
            {
                LOG_ERROR << "http client could not connect to server: " << http_errorstring(resp.error);
                return E_DEFAULT;
            }
            else if (resp.status == 401)
            {
                LOG_ERROR << "http client authorization failed";
                return E_DEFAULT;
            }
            else if (resp.status >= 400 && resp.status != HTTP_BADREQUEST && resp.status != HTTP_NOTFOUND && resp.status != HTTP_INTERNAL)
            {
                LOG_ERROR << "http client error:  server returned HTTP error " << resp.status;
                return E_DEFAULT;
            }
            else if (resp.body.empty())
            {
                LOG_ERROR << "http client error:  no resp from server";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

    }

}