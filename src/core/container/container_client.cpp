/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºcontainer_client.cpp
* description    £ºcontainer client for definition
* date                  : 2018.04.04
* author            £ºBruce Feng
**********************************************************************************/

#include "container_client.h"
#include "common.h"

#include <event2/event.h>
#include <event2/http.h>

#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>


namespace matrix
{
    namespace core
    {

        container_client::container_client(std::string remote_ip, uint16_t remote_port)
            : m_http_client(remote_ip, remote_port)
            , m_remote_ip(remote_ip)
            , m_remote_port(remote_port)
        {

        }

        std::shared_ptr<container_create_resp> container_client::create_container(std::shared_ptr<container_config> config)
        {
            return create_container(config, "");
        }

        std::shared_ptr<container_create_resp> container_client::create_container(std::shared_ptr<container_config> config, std::string name)
        {
            //endpoint
            std::string endpoint = "/containers/create";
            if (!name.empty())
            {
                endpoint += "?name=";
                endpoint += name;
            }

            //req content, headers, resp
            std::shared_ptr<rapidjson::StringBuffer> req_content = config->to_buf();
            kvs headers;
            http_reply resp;
            
            int32_t ret = m_http_client.post(endpoint, headers, *req_content, resp);
            if (E_SUCCESS != ret)
            {
                return nullptr;
            }

             //parse resp
             rapidjson::Document doc;
             doc.Parse<0>(resp.body.c_str());

             std::shared_ptr<container_create_resp> create_resp = std::make_shared<container_create_resp>();

             //id
             rapidjson::Value &id = doc["id"];
             create_resp->container_id = id.GetString();

             //warinings
             rapidjson::Value &warnings = doc["Warnings"];
             if (warnings.IsArray())
             {
                 for (rapidjson::SizeType i = 0; i < warnings.Size(); i++)
                 {
                     const rapidjson::Value& object = warnings[i];
                     create_resp->warnings.push_back(object.GetString());
                 }
             }

             return create_resp;
        }

        int32_t container_client::start_container(std::string container_id)
        {
            return start_container(container_id, nullptr);
        }

        int32_t container_client::start_container(std::string container_id, std::shared_ptr<container_host_config> config)
        {
            //endpoint
            std::string endpoint = "/containers/";
            if (container_id.empty())
            {
                return E_DEFAULT;
            }

            endpoint += container_id;
            endpoint += "/start";

            //req content, headers, resp
            rapidjson::StringBuffer req_content;
            kvs headers;
            http_reply resp;

            int32_t ret = m_http_client.post(endpoint, headers, req_content, resp);
            if (E_SUCCESS != ret)
            {
                //parse resp
                rapidjson::Document doc;
                doc.Parse<0>(resp.body.c_str());

                //id
                rapidjson::Value &message = doc["message"];
                LOG_DEBUG << "start container message: " << message.GetString();
                return ret;
            }

            return E_SUCCESS;
        }

        int32_t container_client::stop_container(std::string container_id)
        {
            return stop_container(container_id, DEFAULT_STOP_CONTAINER_TIME);
        }

        int32_t container_client::stop_container(std::string container_id, int32_t timeout)
        {
            //endpoint
            std::string endpoint = "/containers/";
            if (container_id.empty())
            {
                return E_DEFAULT;
            }

            endpoint += container_id;
            endpoint += "/stop";

            if (timeout > 0)
            {
                endpoint += "?t=";
                endpoint += timeout;
            }

            //req content, headers, resp
            rapidjson::StringBuffer req_content;
            kvs headers;
            http_reply resp;

            int32_t ret = m_http_client.post(endpoint, headers, req_content, resp);
            if (E_SUCCESS != ret)
            {
                //parse resp
                rapidjson::Document doc;
                doc.Parse<0>(resp.body.c_str());

                //id
                rapidjson::Value &message = doc["message"];
                LOG_DEBUG << "start container message: " << message.GetString();
                return ret;
            }

            return E_SUCCESS;
        }

        int32_t container_client::wait_container(std::string container_id)
        {
            //endpoint
            std::string endpoint = "/containers/";
            if (container_id.empty())
            {
                return E_DEFAULT;
            }

            endpoint += container_id;
            endpoint += "/wait";

            //req content, headers, resp
            rapidjson::StringBuffer req_content;
            kvs headers;
            http_reply resp;

            int32_t ret = m_http_client.post(endpoint, headers, req_content, resp);
            if (E_SUCCESS != ret)
            {
                //parse resp
                rapidjson::Document doc;
                doc.Parse<0>(resp.body.c_str());

                //id
                rapidjson::Value &message = doc["message"];
                LOG_DEBUG << "start container error message: " << message.GetString();
                return ret;
            }
            else
            {
                //parse resp
                rapidjson::Document doc;
                doc.Parse<0>(resp.body.c_str());

                //id
                rapidjson::Value &status_code = doc["StatusCode"];
                if (0 != status_code.GetInt())
                {
                    return E_DEFAULT;
                }

                return E_SUCCESS;
            }

            return E_SUCCESS;
        }

        int32_t container_client::remove_container(std::string container_id)
        {
            return remove_container(container_id, false);
        }

        int32_t container_client::remove_container(std::string container_id, bool remove_volumes)
        {
            //endpoint
            std::string endpoint = "/containers/";
            if (container_id.empty())
            {
                return E_DEFAULT;
            }

            endpoint += container_id;

            endpoint += "?v=";
            endpoint += remove_volumes ? "1" : "0";

            //req content, headers, resp
            kvs headers;
            http_reply resp;

            int32_t ret = m_http_client.del(endpoint, headers, resp);
            if (E_SUCCESS != ret)
            {
                //parse resp
                rapidjson::Document doc;
                doc.Parse<0>(resp.body.c_str());

                //id
                rapidjson::Value &message = doc["message"];
                LOG_DEBUG << "start container message: " << message.GetString();
                return ret;
            }

            return E_SUCCESS;
        }

        std::shared_ptr<container_inspect_response> container_client::inspect_container(std::string container_id)
        {
            //endpoint
            std::string endpoint = "/containers/";
            if (container_id.empty())
            {
                return nullptr;
            }

            endpoint += container_id;

            //headers, resp
            kvs headers;
            http_reply resp;

            int32_t ret = m_http_client.del(endpoint, headers, resp);
            if (E_SUCCESS != ret)
            {
                //parse resp
                rapidjson::Document doc;
                doc.Parse<0>(resp.body.c_str());

                //id
                rapidjson::Value &message = doc["message"];
                LOG_DEBUG << "start container message: " << message.GetString();
                return nullptr;
            }
            else
            {
                rapidjson::Document doc;
                doc.Parse<0>(resp.body.c_str());

                std::shared_ptr<container_inspect_response> inspect_resp = std::make_shared<container_inspect_response>();

                rapidjson::Value &state = doc["State"];
                rapidjson::Value &running = state["Running"];
                rapidjson::Value &exit_code = state["ExitCode"];

                inspect_resp->state.running = running.GetBool();
                inspect_resp->state.exit_code = exit_code.GetInt();

                return inspect_resp;
            }
        }

    }

}