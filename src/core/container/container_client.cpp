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

            endpoint += "_DBC_"+autodbcimage_version+"/rename";
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


        std::string container_client::get_commit_image(std::string container_id,std::string version,std::string task_id)
        {
            //endpoint
            std::string endpoint = "/commit?container=";
            if (container_id.empty())
            {
                LOG_ERROR << "commit container container id is empty";
                return "error";
            }

            endpoint += container_id;
            endpoint += "&repo=www.dbctalk.ai:5000/dbc-free-container&tag=autodbcimage_"+task_id.substr(0,6)+"_"+container_id.substr(0,6)+version;

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
                return "error";
            }
            rapidjson::Document doc;
            if (E_SUCCESS != ret)
            {
                LOG_INFO << "commit image error" ;
                return "error";
            }
                //parse resp

            if (!doc.Parse<0>(resp.body.c_str()).HasParseError())
            {
                    //message
                    if (doc.HasMember("message"))
                    {
                        rapidjson::Value &message = doc["message"];
                        LOG_INFO << "commit image error:" << message.GetString();
                        return "error";
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


            return "error";
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

        int32_t container_client::restart_container(std::string container_id)
        {
            if(stop_container(container_id)==E_SUCCESS)
            {
                int32_t ret =start_container(container_id,nullptr);
                return ret;
            } else
            {
                return E_DEFAULT;
            }

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

        int32_t container_client::remove_image(std::string image_id)
        {
            //endpoint
            std::string endpoint = "/images/";
            if (image_id.empty())
            {
                return E_DEFAULT;
            }

            endpoint += image_id;
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
                LOG_ERROR << "container client remove image error: " << e.what();
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
                    LOG_ERROR << "remove image message: " << message.GetString();
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

        std::string container_client::get_image_id(std::string container_id)
        {
            //endpoint
            std::string endpoint = "/containers/";
            if (container_id.empty())
            {
                return "error";
            }

            endpoint += container_id;
            endpoint += "/json";

            //headers, resp
            kvs headers;
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });
            http_response resp;
            int32_t ret = E_SUCCESS;
            std::string image_id ="";
            try
            {
                ret = m_http_client.get(endpoint, headers, resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "container client inspect container error: " << e.what();
                return "error";
            }

            if (E_SUCCESS != ret)
            {

                return "error";
            }
            else
            {
                rapidjson::Document doc;
                if (resp.body.empty()){
                    return "error";
                }

                if (doc.Parse<0>(resp.body.c_str()).HasParseError())
                {
                    LOG_ERROR << "parse inspect_container file error:" << GetParseError_En(doc.GetParseError());
                    return "error";
                }

         //       std::shared_ptr<container_inspect_response> inspect_resp = std::make_shared<container_inspect_response>();

                //message
                if (!doc.HasMember("Image"))
                {
                    return "error";
                }

                rapidjson::Value &image = doc["Image"];

                image_id=image.GetString();
                LOG_ERROR << "get image id: " << image_id;
            }

            return image_id ;
        }

        bool  container_client::can_delete_image(std::string image_id)
        {
            //endpoint
            std::string endpoint = "/images/";
            if (image_id.empty())
            {
                return false;
            }

            endpoint += image_id;
            endpoint += "/json";

            //headers, resp
            kvs headers;
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });
            http_response resp;
            int32_t ret = E_SUCCESS;
            bool can_delete_image=false;
            try
            {
                ret = m_http_client.get(endpoint, headers, resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "container client inspect image error: " << e.what();
                return false;
            }

            if (E_SUCCESS != ret)
            {

                return false;
            }
            else
            {
                rapidjson::Document doc;
                if (resp.body.empty()){
                    return false;
                }

                if (doc.Parse<0>(resp.body.c_str()).HasParseError())
                {
                    LOG_ERROR << "parse inspect_image file error:" << GetParseError_En(doc.GetParseError());
                    return false;
                }

         //       std::shared_ptr<container_inspect_response> inspect_resp = std::make_shared<container_inspect_response>();

                //message
                if (!doc.HasMember("RepoTags"))
                {
                    return false;
                }


                rapidjson::Value &tags = doc["RepoTags"];

                for (rapidjson::Value::ConstValueIterator itr = tags.Begin(); itr != tags.End(); ++itr)
                {
                    if(itr!= nullptr&&itr->IsString())
                    {
                        std::string tag=itr->GetString();
                        if(tag.find("autodbcimage")!= std::string::npos)
                        {
                            LOG_INFO << tag <<"TAG contain:autodbcimage";
                            can_delete_image=true;
                            return can_delete_image;
                        }else{
                            LOG_INFO << tag <<"TAG don't contain:autodbcimage";
                        }
                    }

                }



            }

            return can_delete_image;
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

            LOG_INFO << "inspect image resp:" << resp.status;

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


        std::string container_client::get_container(const std::string user_container_name)
        {
            //endpoint
            std::string endpoint = "/containers/json";
            endpoint += boost::str(boost::format("?filters={\"since\":[\""+user_container_name+"\"]}") );

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
                LOG_ERROR << "get container error: " << endpoint<<e.what();
                return nullptr;
            }
            LOG_INFO << "get container success: " << endpoint;

            if (E_SUCCESS != ret)
            {
                LOG_INFO << "get container info failed: " << resp.body;
                return nullptr;
            }


            std::string container=resp.body;

            return container;
        }

        std::string container_client::get_running_container()
        {
            //endpoint
            std::string endpoint = "/containers/json?";
            endpoint +="size=true";
            endpoint += boost::str(boost::format("&filters={\"status\":[\"running\"]}") );

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
                LOG_ERROR << "get running  containers error: " << endpoint<<e.what();
                return nullptr;
            }
            LOG_INFO << "get running containers success: " << endpoint;

            if (E_SUCCESS != ret)
            {
                LOG_INFO << "get running containers  info failed: " << resp.body;
                return nullptr;
            }


            std::string container=resp.body;

            return container;
        }


        void container_client::del_images()
        {
            //endpoint
            std::string endpoint = "/images/json";

          //  endpoint += boost::str(boost::format("filters={\"dangling\":[\"false\"]}") );
            //  endpoint += boost::str(boost::format("?filters={\"dangling\":[\"true\"]}") );
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
                return ;
            }
            LOG_INFO << "get images success: " << resp.body;

            if (E_SUCCESS != ret)
            {
                // parse resp;
                rapidjson::Document doc;
                doc.Parse<0>(resp.body.c_str());

                ////message
                if (doc.HasMember("message"))
                {
                    rapidjson::Value &message = doc["message"];
                    LOG_ERROR << "images message: " << message.GetString();
                }

            }
            else
            {
                try {
                    rapidjson::Document doc;
                    if (resp.body.empty()) {
                        return;
                    }
                    if (doc.Parse<0>(resp.body.c_str()).HasParseError()) {
                        LOG_ERROR << "parse images file error:" << GetParseError_En(doc.GetParseError());
                        return;
                    }


                    rapidjson::Value &images = doc;

                    for (rapidjson::Value::ConstValueIterator itr = images.Begin(); itr != images.End(); itr++) {
                        if (itr == nullptr)
                        {
                            return;
                        }
                        if (!itr->IsObject())
                        {
                            return;
                        }
                        if (itr->HasMember("Id")) {
                            rapidjson::Value::ConstMemberIterator id = itr->FindMember("Id");

                            std::string id_sha_string = id->value.GetString();

                            std::vector<std::string> list;
                            string_util::split(id_sha_string, ":", list);

                            LOG_INFO << "id_sha_string " << id_sha_string;
                            std::string id_string = list[1];
                            LOG_INFO << "id_string " << id_string;

                            rapidjson::Value::ConstMemberIterator tags = itr->FindMember("RepoTags");

                            rapidjson::Value::ConstMemberIterator time = itr->FindMember("Created");

                            rapidjson::Value::ConstMemberIterator containers = itr->FindMember("Containers");
                            int16_t containers_int = containers->value.GetInt();

                            int64_t t = time->value.GetInt64();
                            LOG_INFO << "time:" << t;
                            LOG_INFO << "containers_int:" << containers_int;
                            if (t < 5000000 || containers_int>0)//时间很短
                            {
                                break;
                            }
                            int size = tags->value.Size();
                            for (int j = 0; j < size; j++) {

                                std::string tag = tags->value[0].GetString();
                                if (tag.find("autodbcimage") !=
                                    std::string::npos)//||tag.find("<none>:<none>")!= std::string::npos
                                {
                                    LOG_INFO << "tag:" << tag;
                                    try {
                                        delete_image(id_string);
                                        break;
                                    }
                                    catch (const std::exception &e) {
                                        LOG_ERROR << "delete image error: " << endpoint << e.what();
                                        break;
                                    }


                                } else {
                                    LOG_INFO << tag << "TAG don't contain:autodbcimage";
                                }
                            }


                        }

                    }


                } catch (const std::exception & e)
                {
                    LOG_ERROR << "get images error: " << endpoint<<e.what();
                    return ;
                }


               }

            return ;

        }


        int32_t container_client::exist_container(const std::string & container_name)
        {
            std::string endpoint = "/containers/";
            if (container_name.empty())
            {
                return E_DEFAULT;
            }

            endpoint += container_name;
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
                return E_DEFAULT;
            }

            LOG_DEBUG << "inspect container resp:" << resp.body;

            if (200 == resp.status)
            {
                return E_SUCCESS;
            }

            if (404 == resp.status)
            {
                return  E_CONTAINER_NOT_FOUND;
            }

            return ret;
        }



    }

}