/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   container_client.cpp
* description    :   container client for definition
* date                  :   2018.04.04
* author            :   Bruce Feng
**********************************************************************************/

#include "container_client.h"
#include "common.h"
#include "document.h"

#include <event2/event.h>
// #include <event2/http.h>

#include <event2/buffer.h>
// #include <event2/keyvalq_struct.h>
#include "error/en.h"
#include "oss_common_def.h"
#include <boost/format.hpp>
namespace matrix
{
    namespace core
    {

        container_client::container_client(std::string remote_ip, uint16_t remote_port)
            : m_http_client(remote_ip, remote_port)
            , m_remote_ip(remote_ip)
            , m_remote_port(remote_port)
            , m_docker_info_ptr(nullptr)
        {

        }

        std::shared_ptr<container_create_resp> container_client::create_container(std::shared_ptr<container_config> config)
        {
            return create_container(config, "","");
        }

        std::shared_ptr<container_create_resp> container_client::create_container(std::shared_ptr<container_config> config, std::string name,std::string autodbcimage_version)
        {
            //endpoint
            std::string endpoint = "/containers/create";


            if (!name.empty())
            {
                endpoint += "?name="+name;

            }

            if(!autodbcimage_version.empty())
            {
                endpoint += "_DBC_"+autodbcimage_version;
            }

            //req content, headers, resp
            std::string && req_content = config->to_string();

            LOG_DEBUG<<"req_content:" << req_content;
            
            kvs headers;
            headers.push_back({"Content-Type", "application/json"});
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });

            http_response resp;
            int32_t ret = E_SUCCESS;
            
            try
            {
                ret = m_http_client.post(endpoint, headers, req_content, resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "container client create container error: " << e.what();
                return nullptr;
            }

            if (E_SUCCESS != ret)
            {
                LOG_INFO << "create container failed: " <<  resp.body << " name:" << name;
                return nullptr;
            }

             //parse resp
             std::shared_ptr<container_create_resp> create_resp = std::make_shared<container_create_resp>();
             create_resp->from_string(resp.body);

             return create_resp;
        }

        int32_t container_client::rename_container(std::string name,std::string autodbcimage_version)
        {
            //endpoint
            std::string endpoint = "/containers/";


            if (!name.empty())
            {
                endpoint += name;

            }

            endpoint += "_DBC_"+autodbcimage_version+"/rename/";
            endpoint += "?name="+name;
            //req content, headers, resp

            kvs headers;
            headers.push_back({"Content-Type", "application/json"});
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });

            http_response resp;
            int32_t ret ;

            try
            {
                ret = m_http_client.post(endpoint, headers, "", resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "container client create container error: " << e.what();

            }


            return ret;
        }


        std::string container_client::get_commit_image(std::string container_id,std::string version)
        {
            //endpoint
            std::string endpoint = "/commit?container=";
            if (container_id.empty())
            {
                LOG_ERROR << "commit container container id is empty";
                return "";
            }

            endpoint += container_id;
            endpoint += "&repo=www.dbctalk.ai:5000/dbc-free-container&tag=autodbcimage_"+container_id.substr(0,12)+version;

            //req content, headers, resp
            std::string req_content = "";
            kvs headers;

            headers.push_back({ "Content-Type", "application/json" });

            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });

            http_response resp;
            int32_t ret;

            try
            {
                ret = m_http_client.post(endpoint, headers, req_content, resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "container client commit container error: " << e.what();
                return "";
            }
            rapidjson::Document doc;
            if (E_SUCCESS != ret)
            {
                return "";
            }
                //parse resp

            if (!doc.Parse<0>(resp.body.c_str()).HasParseError())
            {
                    //message
                    if (doc.HasMember("message"))
                    {
                        rapidjson::Value &message = doc["message"];
                        LOG_ERROR << "commit image error:" << message.GetString();
                        return "";
                    } else
                    {
                        if (doc.HasMember("Id"))
                        {
                            rapidjson::Value &id = doc["Id"];
                            std::string idString=id.GetString();
                            LOG_INFO << "commit image Id: " << idString;

                            return idString;
                        }
                    }

             }


            return "";
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
                LOG_ERROR << "start container container id is empty";
                return E_DEFAULT;
            }

            endpoint += container_id;
            endpoint += "/start";

            //req content, headers, resp
            std::string req_content = "";
            kvs headers;
            if (nullptr != config)
            {
                headers.push_back({ "Content-Type", "application/json" });
            }
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });

            http_response resp;
            int32_t ret = E_SUCCESS;

            try
            {
                ret = m_http_client.post(endpoint, headers, req_content, resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "container client start container error: " << e.what();
                return E_DEFAULT;
            }

            if (E_SUCCESS != ret)
            {
                //parse resp
                rapidjson::Document doc;
                if (!doc.Parse<0>(resp.body.c_str()).HasParseError())
                {
                    //message
                    if (doc.HasMember("message"))
                    {
                        rapidjson::Value &message = doc["message"];
                        LOG_ERROR << "start container message: " << message.GetString();
                    }

                }

                return ret;
            }

            return E_SUCCESS;
        }

        int32_t container_client::update_container(std::string container_id, std::shared_ptr<update_container_config> config)
        {
            //endpoint
            std::string endpoint = "/containers/";
            if (container_id.empty())
            {
                LOG_ERROR << "updata container container id is empty";
                return E_DEFAULT;
            }

            endpoint += container_id;
            endpoint += "/update";

            //req content, headers, resp
            //req content, headers, resp
            std::string && req_content = config->update_to_string();

            LOG_DEBUG<<"req_content:" << req_content;
            kvs headers;
            if (nullptr != config)
            {
                headers.push_back({ "Content-Type", "application/json" });
            }
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });

            http_response resp;
            int32_t ret = E_SUCCESS;

            try
            {
                ret = m_http_client.post(endpoint, headers, req_content, resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "container client updata container error: " << e.what();
                return E_DEFAULT;
            }

            if (E_SUCCESS != ret)
            {
                //parse resp
                rapidjson::Document doc;
                if (!doc.Parse<0>(resp.body.c_str()).HasParseError())
                {
                    //message
                    if (doc.HasMember("message"))
                    {
                        rapidjson::Value &message = doc["message"];
                        LOG_ERROR << "update container message: " << message.GetString();
                    }

                }

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
                endpoint += std::to_string(timeout);
            }

            //req content, headers, resp
            std::string req_content;
            kvs headers;
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });

            http_response resp;
            int32_t ret = E_SUCCESS;

            try
            {
                ret = m_http_client.post(endpoint, headers, req_content, resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "container client stop container error: " << e.what();
                return E_DEFAULT;
            }

            if (E_SUCCESS != ret)
            {
                //parse resp
                //rapidjson::Document doc;
                //doc.Parse<0>(resp.body.c_str());

                ////message
                //if (doc.HasMember("message"))
                //{
                //    rapidjson::Value &message = doc["message"];
                //    LOG_ERROR << "stop container message: " << message.GetString();
                //}
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
            std::string req_content;
            kvs headers;
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });
            http_response resp;
            int32_t ret = E_SUCCESS;

            try
            {
                ret = m_http_client.post(endpoint, headers, req_content, resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "container client create container error: " << e.what();
                return E_DEFAULT;
            }

            if (E_SUCCESS != ret)
            {
                //parse resp
                //rapidjson::Document doc;
                //doc.Parse<0>(resp.body.c_str());

                ////message
                //if (doc.HasMember("message"))
                //{
                //    rapidjson::Value &message = doc["message"];
                //    LOG_ERROR << "wait container error message: " << message.GetString();
                //}
                return ret;
            }
            else
            {
                //parse resp
                rapidjson::Document doc;
                //doc.Parse<0>(resp.body.c_str());
                if (doc.Parse<0>(resp.body.c_str()).HasParseError())
                {
                    LOG_ERROR << "parse wait_container file error:" << GetParseError_En(doc.GetParseError());
                    return E_DEFAULT;
                }

                //StatusCode
                if (! doc.HasMember("StatusCode"))
                {
                    return E_DEFAULT;
                }

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

           // endpoint += "?v=";
           // endpoint += remove_volumes ? "1" : "0";

            //req content, headers, resp
            kvs headers;
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });
            http_response resp;
            int32_t ret = E_SUCCESS;

            try
            {
                ret = m_http_client.del(endpoint, headers, resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "container client remove container error: " << e.what();
                return E_DEFAULT;
            }

            if (E_SUCCESS != ret)
            {
               // parse resp;
                rapidjson::Document doc;
                doc.Parse<0>(resp.body.c_str());

                ////message
                if (doc.HasMember("message"))
                {
                    rapidjson::Value &message = doc["message"];
                    LOG_ERROR << "remove container message: " << message.GetString();
                }
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
            endpoint += "/json";

            //headers, resp
            kvs headers;
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });
            http_response resp;
            int32_t ret = E_SUCCESS;

            try
            {
                ret = m_http_client.get(endpoint, headers, resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "container client inspect container error: " << e.what();
                return nullptr;
            }

            if (E_SUCCESS != ret)
            {
                //parse resp
                //rapidjson::Document doc;
                //doc.Parse<0>(resp.body.c_str());

                ////message
                //if (doc.HasMember("message"))
                //{
                //    rapidjson::Value &message = doc["message"];
                //    LOG_ERROR << "inspect container message: " << message.GetString();
                //}
                return nullptr;
            }
            else
            {
                rapidjson::Document doc;
                //doc.Parse<0>(resp.body.c_str());
                if (doc.Parse<0>(resp.body.c_str()).HasParseError())
                {
                    LOG_ERROR << "parse inspect_container file error:" << GetParseError_En(doc.GetParseError());
                    return nullptr;
                }

                std::shared_ptr<container_inspect_response> inspect_resp = std::make_shared<container_inspect_response>();
                
                //message
                if (!doc.HasMember("State"))
                {
                    return nullptr;
                }

                rapidjson::Value &state = doc["State"];

                //running state
                if (state.HasMember("Running"))
                {
                    rapidjson::Value &running = state["Running"];
                    inspect_resp->state.running = running.GetBool();
                }

                //exit code
                if (state.HasMember("ExitCode"))
                {
                    rapidjson::Value &exit_code = state["ExitCode"];
                    inspect_resp->state.exit_code = exit_code.GetInt();
                }                
                
                if (E_SUCCESS != parse_item_string(doc, "Id", inspect_resp->id))
                {
                    return nullptr;
                }

                return inspect_resp;
            }
        }

        std::shared_ptr<container_logs_resp> container_client::get_container_log(std::shared_ptr<container_logs_req> req)
        {
            if (nullptr == req)
            {
                LOG_ERROR << "get container log nullptr";
                return nullptr;
            }

            //endpoint
            std::string endpoint = "/containers/";
            if (req->container_id.empty())
            {
                LOG_ERROR << "get container log container id empty";
                return nullptr;
            }

            endpoint += req->container_id;
            endpoint += "/logs?stderr=1&stdout=1";

            //timestamp
            if (true == req->timestamps)
            {
                endpoint += "&timestamps=true";
            }

            //get log from tail
            if (1 == req->head_or_tail)
            {
                endpoint += (0 == req->number_of_lines) ? "&tail=all" : "&tail=" + std::to_string(req->number_of_lines);
            }

            //headers, resp
            kvs headers;
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });
            //headers.push_back({"Accept-Charset", "utf-8"});
            http_response resp;
            int32_t ret = E_SUCCESS;

            try
            {
                ret = m_http_client.get(endpoint, headers, resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "container client get container logs error: " << e.what();
                return nullptr;
            }

            if (E_SUCCESS != ret)
            {
                return nullptr;
            }
            else
            {
                std::shared_ptr<container_logs_resp> logs_resp = std::make_shared<container_logs_resp>();
                logs_resp->log_content = resp.body;

                return logs_resp;
            }
        }


        void container_client::set_address(std::string remote_ip, uint16_t remote_port)
        {
            m_remote_ip = remote_ip;
            m_remote_port = remote_port;
            m_http_client.set_address(remote_ip, remote_port);
        }

        int32_t container_client::exist_docker_image(const std::string & image_name)
        {
            std::string endpoint = "/images/";
            if (image_name.empty())
            {
                return E_DEFAULT;
            }

            endpoint += image_name;
            endpoint += "/json";

            //headers, resp
            kvs headers;
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });
            http_response resp;
            int32_t ret = E_SUCCESS;

            try
            {
                ret = m_http_client.get(endpoint, headers, resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "container client inspect image error: " << e.what();
                return E_DEFAULT;
            }

            LOG_DEBUG << "inspect image resp:" << resp.body;

            if (200 == resp.status)
            {
                return E_SUCCESS;
            }

            if (404 == resp.status)
            {
                return E_IMAGE_NOT_FOUND;
            }

            return ret;
        }

        std::shared_ptr<docker_info> container_client::get_docker_info()
        {
            if (m_docker_info_ptr) return m_docker_info_ptr;

            std::string endpoint = "/info";
            //headers, resp
            kvs headers;
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });
            http_response resp;
            int32_t ret = E_SUCCESS;

            try
            {
                ret = m_http_client.get(endpoint, headers, resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "container client get docker info error: " << e.what();
                return nullptr;
            }

            if (E_SUCCESS != ret)
            {
                LOG_DEBUG << "get docker info failed: " << resp.body;
                return nullptr;
            }

            LOG_DEBUG << "get docker info: " << resp.body;

            //parse resp
            std::shared_ptr<docker_info> docker_ptr = std::make_shared<docker_info>();
            docker_ptr->from_string(resp.body);

            m_docker_info_ptr = docker_ptr;
            return docker_ptr;
        }


        int32_t container_client::prune_container(int16_t interval)
        {
            //endpoint
            std::string endpoint = "/containers/prune";

            if (interval > 0)
            {

                endpoint += boost::str(boost::format("?filters={\"until\":[\"%dh\"]}") % interval);
            }

            //headers, resp
            kvs headers;
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });
            http_response resp;
            int32_t ret = E_SUCCESS;

            try
            {
                ret = m_http_client.post(endpoint, headers, "",resp);

            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "prun container error: " << endpoint<<e.what();
                return E_DEFAULT;
            }
            LOG_INFO << "prun container success: " << endpoint;
            return ret;
        }

        std::shared_ptr<images_info>  container_client::get_images()
        {
            //endpoint
            std::string endpoint = "/images/json";


            //headers, resp
            kvs headers;
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });
            http_response resp;
            int32_t ret;

            try
            {
                ret = m_http_client.get(endpoint, headers,resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "get images error: " << endpoint<<e.what();
                return nullptr;
            }
            LOG_INFO << "get images success: " << endpoint;

            if (E_SUCCESS != ret)
            {
                LOG_DEBUG << "get images info failed: " << resp.body;
                return nullptr;
            }

            //parse resp
            std::shared_ptr<images_info> list_images_info_ptr = std::make_shared<images_info>();
            list_images_info_ptr->from_string(resp.body);

            return list_images_info_ptr;
        }

        int32_t container_client::delete_image(std::string id)
        {
            //endpoint
            std::string endpoint = "/images/";


            endpoint += id;


            //headers, resp
            kvs headers;
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });
            http_response resp;
            int32_t ret;

            try
            {
                ret = m_http_client.del(endpoint, headers, resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "delete image error: " << endpoint<<e.what();
                return E_DEFAULT;
            }
            if(ret==E_SUCCESS)
            {
                LOG_INFO << "delete image success: " << endpoint;
            }

            return ret;
        }

        int32_t container_client::prune_images()
        {
            //endpoint
            std::string endpoint = "/images/prune";



            endpoint += boost::str(boost::format("?filters={\"dangling\":[\"true\"]}") );


            //headers, resp
            kvs headers;
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });
            http_response resp;
            int32_t ret;

            try
            {
                ret = m_http_client.post(endpoint, headers, "",resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "prun images error: " << endpoint<<e.what();
                return E_DEFAULT;
            }
            LOG_INFO << "prun images success: " << endpoint;
            return ret;
        }

    }

}