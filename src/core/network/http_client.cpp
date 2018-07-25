/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   http_client.cpp
* description    :   http client for rpc
* date                  :   2018.04.07
* author             :   Bruce Feng
**********************************************************************************/

#include "http_client.h"
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include <event2/http.h>
#include <boost/format.hpp>
#include <openssl/ssl.h>
#include <openssl/tls1.h>
#include <openssl/rand.h>
#include <event2/bufferevent_ssl.h>

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
            http_response *reply = static_cast<http_response*>(ctx);

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
            http_response *reply = static_cast<http_response*>(ctx);
            reply->error = err;
        }

        http_client::http_client(std::string remote_ip, uint16_t remote_port)
            : m_remote_ip(remote_ip)
            , m_remote_port(remote_port)
            , m_uri("")
            , m_scheme("http")
        {
            //add_ssl_engine();
        }

        http_client::http_client(std::string &url)
            : m_uri("")
            , m_scheme("http")
        {
            parse_url(url);
            //add_ssl_engine();
        }

        void http_client::set_remote(std::string remote_ip, uint16_t remote_port)
        {
            m_remote_ip = remote_ip;
            m_remote_port = remote_port;
        }

        int32_t http_client::post(const std::string &endpoint, const kvs &headers, const std::string & req_content, http_response &resp)
        {
            //event base
            raii_event_base base = obtain_event_base();
            bufferevent * bev = add_ssl_engine(base.get());
            //connection base
            raii_evhttp_connection evcon = obtain_evhttp_connection_base2(base.get(), bev, m_remote_ip, m_remote_port);
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
            //std::string strRequest = req_content;
            struct evbuffer* output_buffer = evhttp_request_get_output_buffer(req.get());
            assert(output_buffer);
            evbuffer_add(output_buffer, req_content.data(), req_content.size());

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
            else if (resp.status >= 400) //&& resp.status != HTTP_BADREQUEST && resp.status != HTTP_NOTFOUND && resp.status != HTTP_INTERNAL)
            {
                LOG_ERROR << "http client error: server returned HTTP error " << resp.status;
                return E_DEFAULT;
            }
            /*else if (resp.body.empty())
            {
                LOG_ERROR << "http client error: no resp from server";
                return E_DEFAULT;
            }*/        

            return E_SUCCESS;
        }

        int32_t http_client::get(const std::string &endpoint, const kvs &headers, http_response &resp)
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
            else if (resp.status >= 400) //&& resp.status != HTTP_BADREQUEST && resp.status != HTTP_NOTFOUND && resp.status != HTTP_INTERNAL)
            {
                LOG_ERROR << "http client error: server returned HTTP error " << resp.status;
                return E_DEFAULT;
            }
            /*else if (resp.body.empty())
            {
                LOG_ERROR << "http client error: no resp from server";
                return E_DEFAULT;
            }*/

            return E_SUCCESS;
        }

        int32_t http_client::del(const std::string &endpoint, const kvs &headers, http_response &resp)
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
            else if (resp.status >= 400) //&& resp.status != HTTP_BADREQUEST && resp.status != HTTP_NOTFOUND && resp.status != HTTP_INTERNAL)
            {
                LOG_ERROR << "http client error: server returned HTTP error " << resp.status;
                return E_DEFAULT;
            }
            /*else if (resp.body.empty())
            {
                LOG_ERROR << "http client error: no resp from server";
                return E_DEFAULT;
            }*/

            return E_SUCCESS;
        }
        typedef enum {
            MatchFound,
            MatchNotFound,
            NoSANPresent,
            MalformedCertificate,
            Error
        } HostnameValidationResult;
        static int cert_verify_callback(X509_STORE_CTX *x509_ctx, void *arg)
        {
            char cert_str[256];
            const char *host = (const char *)arg;
            const char *res_str = "X509_verify_cert failed";
            HostnameValidationResult res = Error;

            /* This is the function that OpenSSL would call if we hadn't called
            * SSL_CTX_set_cert_verify_callback().  Therefore, we are "wrapping"
            * the default functionality, rather than replacing it. */
            int ok_so_far = 0;

            X509 *server_cert = NULL;
            static int ignore_cert = 1;
            if (ignore_cert) {
                return 1;
            }

            ok_so_far = X509_verify_cert(x509_ctx);

            server_cert = X509_STORE_CTX_get_current_cert(x509_ctx);

            if (ok_so_far) {
                //res = validate_hostname(host, server_cert);

                switch (res) {
                case MatchFound:
                    res_str = "MatchFound";
                    break;
                case MatchNotFound:
                    res_str = "MatchNotFound";
                    break;
                case NoSANPresent:
                    res_str = "NoSANPresent";
                    break;
                case MalformedCertificate:
                    res_str = "MalformedCertificate";
                    break;
                case Error:
                    res_str = "Error";
                    break;
                default:
                    res_str = "WTF!";
                    break;
                }
            }

            X509_NAME_oneline(X509_get_subject_name(server_cert),
                cert_str, sizeof(cert_str));

            if (res == MatchFound) {
                printf("https server '%s' has this certificate, "
                    "which looks good to me:\n%s\n",
                    host, cert_str);
                return 1;
            }
            else {
                printf("Got '%s' for hostname '%s' and certificate:\n%s\n",
                    res_str, host, cert_str);
                return 0;
            }
        }

        bufferevent * http_client::add_ssl_engine(event_base *base)
        {
            struct bufferevent *bev;
            struct evhttp_connection *evcon = NULL;
            if (m_scheme == "http")
            {
                bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
                return bev;
            }
            else
            {
#ifdef __linux__
                const char *crt = "/etc/ssl/certs/ca-certificates.crt";
                int32_t random = RAND_poll();
                SSL_CTX *ssl_ctx = NULL;
                SSL *ssl = NULL;
                ssl_ctx = SSL_CTX_new(SSLv23_method());
                if (!ssl_ctx)
                {
                    std::cout << "SSL_CTX_new";
                    return nullptr;
                }
#ifndef _WIN32
                if (1 != SSL_CTX_load_verify_locations(ssl_ctx, crt, NULL))
                {
                    std::cout << "SSL_CTX_load_verify_locations";
                    return nullptr;
                }
#endif
                ssl = SSL_new(ssl_ctx);
                if (ssl == NULL)
                {
                    std::cout << "SSL_new()";
                    return nullptr;
                }

#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
                // Set hostname for SNI extension
                SSL_set_tlsext_host_name(ssl, m_remote_ip.c_str());
                SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, NULL);
                SSL_CTX_set_cert_verify_callback(ssl_ctx, cert_verify_callback, (void *)m_remote_ip.c_str());
#endif


                bev = bufferevent_openssl_socket_new(base, -1, ssl, BUFFEREVENT_SSL_CONNECTING, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);

                bufferevent_openssl_set_allow_dirty_shutdown(bev, 1);
                //bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
                return bev;
#endif
            }
            
            return nullptr;
        }

        int32_t http_client::parse_url(const std::string & url)
        {
            struct evhttp_uri *http_uri = evhttp_uri_parse(url.c_str());

            if (nullptr == http_uri)
            {
                return E_DEFAULT;
            }
            std::string host;
            std::string path;
            const char * query = nullptr;

            m_scheme = evhttp_uri_get_scheme(http_uri);
            m_remote_ip = evhttp_uri_get_host(http_uri);
            int32_t port = evhttp_uri_get_port(http_uri);

            if (port == -1) 
            {
                port = (m_scheme == "http") ? 80 : 443;
            }
            try 
            {
                m_remote_port = port;
                path = evhttp_uri_get_path(http_uri);
                query = evhttp_uri_get_query(http_uri);

                if (query == nullptr)
                {
                    m_uri = boost::str(boost::format("%s") % path);
                }
                else
                {
                    m_uri = boost::str(boost::format("%s?%s") % path % query);
                }
            }
            catch (const std::exception & e)
            {
                LOG_DEBUG <<"parse error:" <<  e.what();
            }
            catch (...)
            {
                LOG_DEBUG << "parse error";
            }
            
            return E_SUCCESS;
        }

        /*bufferevent * http_client::create_bev(event_base *base, SSL *ssl)
        {
            if (m_scheme == "http")
            {
                return bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
            }
            else if (m_scheme == "https")
            {
                return bufferevent_openssl_socket_new(base, -1, ssl, BUFFEREVENT_SSL_CONNECTING, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
            }
            else
            {
                return nullptr;
            }
        }*/

    }

}