/*********************************************************************************
*  Copyright (c) 2017-2021 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   vm_client.cpp
* description    :   vm client for definition
* date                  :   2021.02.08
* author            :   Feng
**********************************************************************************/

#include "vm_client.h"
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

        vm_client::vm_client(std::string remote_ip, uint16_t remote_port)
            : m_http_client(remote_ip, remote_port)
            , m_remote_ip(remote_ip)
            , m_remote_port(remote_port)
            , m_docker_info_ptr(nullptr)
        {

        }

        std::shared_ptr<vm_create_resp> vm_client::create_vm(std::shared_ptr<vm_config> config)
        {
            return create_vm(config, "","");
        }

        std::shared_ptr<vm_create_resp> vm_client::create_vm(std::shared_ptr<vm_config> config, std::string name,std::string autodbcimage_version)
        {
            //endpoint
            std::string endpoint = "/vms/create";


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
                LOG_ERROR << "vm client create vm error: " << e.what();
                return nullptr;
            }

            if (E_SUCCESS != ret)
            {
                LOG_INFO << "create vm failed: " <<  resp.body << " name:" << name;
                return nullptr;
            }

             //parse resp
             std::shared_ptr<vm_create_resp> create_resp = std::make_shared<vm_create_resp>();
             create_resp->from_string(resp.body);

             return create_resp;
        }





        int32_t vm_client::restart_vm(std::string vm_id)
        {
            if(stop_vm(vm_id)==E_SUCCESS)
            {
                int32_t ret =start_vm(vm_id,nullptr);
                return ret;
            } else
            {
                return E_DEFAULT;
            }

        }


        int32_t vm_client::update_vm(std::string vm_id, std::shared_ptr<update_vm_config> config)
        {
            //endpoint
            std::string endpoint = "/vms/";
            if (vm_id.empty())
            {
                LOG_ERROR << "updata vm vm id is empty";
                return E_DEFAULT;
            }

            endpoint += vm_id;
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
                LOG_ERROR << "vm client updata vm error: " << e.what();
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
                        LOG_ERROR << "update vm message: " << message.GetString();
                    }

                }

                return ret;
            }

            return E_SUCCESS;
        }


        int32_t vm_client::stop_vm(std::string vm_id)
        {
            return stop_vm(vm_id, DEFAULT_STOP_CONTAINER_TIME);
        }

        int32_t vm_client::stop_vm(std::string vm_id, int32_t timeout)
        {
            //endpoint
            std::string endpoint = "/vms/";
            if (vm_id.empty())
            {
                return E_DEFAULT;
            }

            endpoint += vm_id;
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
                LOG_ERROR << "vm client stop vm error: " << e.what();
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
                //    LOG_ERROR << "stop vm message: " << message.GetString();
                //}
                return ret;
            }

            return E_SUCCESS;
        }


        int32_t vm_client::remove_vm(std::string vm_id, bool remove_volumes)
        {
            //endpoint
            std::string endpoint = "/vms/";
            if (vm_id.empty())
            {
                return E_DEFAULT;
            }

            endpoint += vm_id;

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
                LOG_ERROR << "vm client remove vm error: " << e.what();
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
                    LOG_ERROR << "remove vm message: " << message.GetString();
                }
                return ret;
            }
            LOG_ERROR << "remove vm message success";
            return E_SUCCESS;
        }

        int32_t vm_client::remove_image(std::string image_id)
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
                LOG_ERROR << "vm client remove image error: " << e.what();
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

        std::shared_ptr<vm_inspect_response> vm_client::inspect_vm(std::string vm_id)
        {
            //endpoint
            std::string endpoint = "/vms/";
            if (vm_id.empty())
            {
                return nullptr;
            }

            endpoint += vm_id;
            endpoint += "/json";

            //headers, resp
            kvs headers;
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });
            http_response resp;
            int32_t ret = E_SUCCESS;

            try
            {
                ret = m_http_client.get_sleep(endpoint, headers, resp,30);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "vm client inspect vm error: " << e.what();
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
                //    LOG_ERROR << "inspect vm message: " << message.GetString();
                //}
                return nullptr;
            }
            else
            {
                rapidjson::Document doc;
                //doc.Parse<0>(resp.body.c_str());
                if (doc.Parse<0>(resp.body.c_str()).HasParseError())
                {
                    LOG_ERROR << "parse inspect_vm file error:" << GetParseError_En(doc.GetParseError());
                    return nullptr;
                }

                std::shared_ptr<vm_inspect_response> inspect_resp = std::make_shared<vm_inspect_response>();
                
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



        std::shared_ptr<vm_logs_resp> vm_client::get_vm_log(std::shared_ptr<vm_logs_req> req)
        {
            if (nullptr == req)
            {
                LOG_ERROR << "get vm log nullptr";
                return nullptr;
            }

            //endpoint
            std::string endpoint = "/vms/";
            if (req->vm_id.empty())
            {
                LOG_ERROR << "get vm log vm id empty";
                return nullptr;
            }

            endpoint += req->vm_id;
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
                LOG_ERROR << "vm client get vm logs error: " << e.what();
                return nullptr;
            }

            if (E_SUCCESS != ret)
            {
                return nullptr;
            }
            else
            {
                std::shared_ptr<vm_logs_resp> logs_resp = std::make_shared<vm_logs_resp>();
                logs_resp->log_content = resp.body;

                return logs_resp;
            }
        }


        void vm_client::set_address(std::string remote_ip, uint16_t remote_port)
        {
            m_remote_ip = remote_ip;
            m_remote_port = remote_port;
            m_http_client.set_address(remote_ip, remote_port);
        }





        int32_t vm_client::prune_vm(int16_t interval)
        {
            //endpoint
            std::string endpoint = "/vms/prune";

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
                LOG_ERROR << "prun vm error: " << endpoint<<e.what();
                return E_DEFAULT;
            }
            LOG_INFO << "prun vm success: " << endpoint;
            return ret;
        }

        std::shared_ptr<images_info>  vm_client::get_images()
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

        int32_t vm_client::delete_image(std::string id)
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

        int32_t vm_client::prune_images()
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


        std::string vm_client::get_vm(const std::string user_vm_name)
        {
            //endpoint
            std::string endpoint = "/vms/json";
            endpoint += boost::str(boost::format("?filters={\"since\":[\""+user_vm_name+"\"]}") );

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
                LOG_ERROR << "get vm error: " << endpoint<<e.what();
                return "error";
            }
            LOG_INFO << "get vm success: " << endpoint;

            if (E_SUCCESS != ret)
            {
                LOG_INFO << "get vm info failed: " << resp.body;
                return "error";
            }


            std::string vm=resp.body;

            return vm;
        }

        std::string vm_client::get_running_vm()
        {
            //endpoint
            std::string endpoint = "/vms/json?";
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
                LOG_ERROR << "get running  vms error: " << endpoint<<e.what();
                return "error";
            }


            if (E_SUCCESS != ret)
            {
                LOG_INFO << "get running vms  info failed: " << resp.body;
                return "error";
            }




            if (resp.body.empty()){
                return "";
            }
            rapidjson::Document doc;
            if (doc.Parse<0>(resp.body.c_str()).HasParseError())
            {
                LOG_ERROR << "get running vms  error:" << GetParseError_En(doc.GetParseError());
                return "";
            }

            LOG_INFO << "get running vms success: " << endpoint;
          //  LOG_INFO << "body " << resp.body;

            rapidjson::Document document;
            rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

            rapidjson::Value root(rapidjson::kArrayType);

            rapidjson::Value &vms = doc;
            for (rapidjson::Value::ConstValueIterator itr = vms.Begin(); itr != vms.End(); itr++) {
                if (itr == nullptr)
                {
                    return "";
                }
                if (!itr->IsObject())
                {
                    return "";
                }
                if (!itr->HasMember("SizeRw")) {
                        continue;
                }else{
                    rapidjson::Value vm(rapidjson::kObjectType);


                    rapidjson::Value::ConstMemberIterator id = itr->FindMember("Id");
                    std::string Id_string=id->value.GetString();
                    vm.AddMember("Id", STRING_DUP(Id_string), allocator);
                  //  LOG_INFO << "Id " <<Id_string;

                    rapidjson::Value::ConstMemberIterator Names = itr->FindMember("Names");
                    std::string  Names_string= Names->value[0].GetString();
                    rapidjson::Value names_array(rapidjson::kArrayType);
                    names_array.PushBack(STRING_DUP(Names_string), allocator);
                    vm.AddMember("Names",names_array , allocator);

                    rapidjson::Value::ConstMemberIterator Image =itr->FindMember("Image");
                    std::string Image_string=Image->value.GetString();
                    vm.AddMember("Image", STRING_DUP(Image_string), allocator);

                    rapidjson::Value::ConstMemberIterator ImageID =itr->FindMember("ImageID");
                    std::string ImageID_string=ImageID->value.GetString();
                    vm.AddMember("ImageID", STRING_DUP(ImageID_string), allocator);


                    rapidjson::Value::ConstMemberIterator Created = itr->FindMember("Created");
                    int64_t Created_int64=Created->value.GetInt64();
                    vm.AddMember("Created", Created_int64, allocator);


                    rapidjson::Value::ConstMemberIterator SizeRw =itr->FindMember("SizeRw");
                    int64_t SizeRw_int64=SizeRw->value.GetInt64();

                    vm.AddMember("SizeRw", SizeRw_int64, allocator);


                    rapidjson::Value::ConstMemberIterator SizeRootFs = itr->FindMember("SizeRootFs");
                    int64_t SizeRootFs_int64=SizeRootFs->value.GetInt64();
                    vm.AddMember("SizeRootFs", SizeRootFs_int64, allocator);

                    root.PushBack(vm.Move(), allocator);



                }
            }



            std::shared_ptr<rapidjson::StringBuffer> buffer = std::make_shared<rapidjson::StringBuffer>();
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(*buffer);
            root.Accept(writer);
            std::string new_vms=std::string(buffer->GetString());
          //  LOG_INFO << "new_vms " <<new_vms;
            return new_vms;
        }


        void vm_client::del_images()
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

                            rapidjson::Value::ConstMemberIterator vms = itr->FindMember("Containers");
                            int16_t vms_int = vms->value.GetInt();

                            int64_t t = time->value.GetInt64();
                            LOG_INFO << "time:" << t;
                            LOG_INFO << "vms_int:" << vms_int;
                            if (t < 5000000 || vms_int>0)//时间很短
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


        int32_t vm_client::exist_vm(const std::string & vm_name)
        {
            std::string endpoint = "/vms/";
            if (vm_name.empty())
            {
                return E_DEFAULT;
            }

            endpoint += vm_name;
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
                LOG_ERROR << "vm client inspect vm error: " << e.what();
                return E_DEFAULT;
            }

            LOG_DEBUG << "inspect vm resp:" << resp.body;

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


        std::string vm_client::get_vm_id(std::string vm_name)
        {
            //endpoint
            std::string endpoint = "/vms/";
            if (vm_name.empty())
            {
                return "error";
            }

            endpoint += vm_name;
            endpoint += "/json";

            //headers, resp
            kvs headers;
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });
            http_response resp;
            int32_t ret;

            try
            {
                ret = m_http_client.get(endpoint, headers, resp);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "vm client inspect vm error: " << e.what();
                return "error";
            }

            if (E_SUCCESS != ret)
            {

                  return "error";
            }
            else
            {
                rapidjson::Document doc;
                //doc.Parse<0>(resp.body.c_str());
                if (doc.Parse<0>(resp.body.c_str()).HasParseError())
                {
                    LOG_ERROR << "parse inspect_vm file error:" << GetParseError_En(doc.GetParseError());
                    return "error";
                }



                //message
                if (!doc.HasMember("State"))
                {
                    return "error";
                }



                //running state
                if (doc.HasMember("Id"))
                {
                    rapidjson::Value &Id = doc["Id"];
                    std::string idString=Id.GetString();
                    return idString;
                }



                return "error";

            }
        }











        std::string vm_client::get_gpu_id(std::string task_id)
        {
            std::string vm_id=get_vm_id_current(task_id);
            //endpoint
            std::string endpoint = "/vms/";
            if (vm_id.empty())
            {
                return "";
            }

            endpoint += vm_id;
            endpoint += "/json";

            //headers, resp
            kvs headers;
            headers.push_back({ "Host", m_remote_ip + ":" + std::to_string(m_remote_port) });
            http_response resp;
            int32_t ret = E_SUCCESS;

            try
            {
                ret = m_http_client.get_sleep(endpoint, headers, resp,10);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "vm client inspect vm error: " << e.what();
                return "";
            }

            if (E_SUCCESS != ret)
            {

                return "";
            }
            else
            {
                rapidjson::Document doc;
                //doc.Parse<0>(resp.body.c_str());
                if (doc.Parse<0>(resp.body.c_str()).HasParseError())
                {
                    LOG_ERROR << "parse inspect_vm file error:" << GetParseError_En(doc.GetParseError());
                    return nullptr;
                }

                if (resp.body.empty()){
                    return "";
                }


             //   LOG_INFO << "parse inspect_vm success: " <<resp.body ;

                //message
                if (!doc.HasMember("Config"))
                {
                    LOG_INFO << "!doc.HasMember(Config) " ;
                    return "";
                }
                try
                {
                    rapidjson::Value &Config = doc["Config"];


                        if (Config.HasMember("Env"))
                        {
                            rapidjson::Value &Env = Config["Env"];
                            for (rapidjson::Value::ConstValueIterator itr = Env.Begin(); itr != Env.End(); ++itr)
                            {
                                if(itr!= nullptr&&itr->IsString())
                                {
                                    std::string gpu_id=itr->GetString();
                                    if(gpu_id.find("NVIDIA_VISIBLE_DEVICES=")!= std::string::npos)
                                    {

                                        LOG_INFO << "NVIDIA_VISIBLE_DEVICES: " <<gpu_id ;
                                        return gpu_id;
                                    }
                                }

                            }


                        }


                }

                catch (const std::exception & e)
                {
                    LOG_ERROR << "get HostConfig error: " << endpoint<<e.what();
                    return "";
                }
            }

            return "";
        }



    }

}