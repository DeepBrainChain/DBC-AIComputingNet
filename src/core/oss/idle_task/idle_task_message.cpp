/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   container_message.h
* description    :   container message definition
* date                  :   2018.04.07
* author            :   Bruce Feng
**********************************************************************************/

#include "idle_task_message.h"
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include "error/en.h"

namespace matrix
{
    namespace core
    {
        std::string idle_task_req::to_string()
        {
            rapidjson::Document document;
            rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

            rapidjson::Value root(rapidjson::kObjectType);

            root.AddMember("mining_node_id", STRING_REF(mining_node_id), allocator);
            root.AddMember("time_stamp", STRING_REF(time_stamp), allocator);
           
            root.AddMember("sign_type", STRING_REF(sign_type), allocator);
            root.AddMember("sign", STRING_REF(sign), allocator);

            std::shared_ptr<rapidjson::StringBuffer> buffer = std::make_shared<rapidjson::StringBuffer>();
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(*buffer);
            root.Accept(writer);

            return buffer->GetString();
        }

        void idle_task_resp::from_string(const std::string & buf)
        {
            try
            {
                rapidjson::Document doc;
                //doc.Parse<0>(buf.c_str());              //left to later not all fields set
                if (doc.Parse<0>(buf.c_str()).HasParseError())
                {
                    LOG_ERROR << "parse idle_task_resp error:" << GetParseError_En(doc.GetParseError());
                    return;
                }
                //message
                if (!doc.HasMember("status"))
                {
                    this->status = OSS_NET_ERROR;
                    LOG_ERROR << "parse idle_task_resp  error. Do not have status";
                    return;
                }
               
                rapidjson::Value &status = doc["status"];
                auto  ret_type = status.GetType();
                if (ret_type != rapidjson::kNumberType)
                {
                    this->status = OSS_NET_ERROR;
                    return;
                }

                this->status = status.GetInt64();
                if (this->status != OSS_SUCCESS)
                {
                    return;
                }

                //idle_task_id
                if (!doc.HasMember("idle_task_id"))
                {
                    LOG_ERROR << "parse idle_task_resp error. Do not have idle_task_id";
                    this->status = OSS_NET_ERROR;
                    return;
                }
                rapidjson::Value &idle_task_id = doc["idle_task_id"];
                ret_type = idle_task_id.GetType();
                if (ret_type != rapidjson::kStringType)
                {
                    this->status = OSS_NET_ERROR;
                    return;
                }
                this->idle_task_id = idle_task_id.GetString();

                if (this->idle_task_id == "0")
                {
                    return;
                }

                //code_dir
                if (!doc.HasMember("code_dir"))
                {
                    LOG_ERROR << "parse code_dir error. Do not have code_dir";
                    this->status = OSS_NET_ERROR;
                    return;
                }
                rapidjson::Value &code_dir = doc["code_dir"];
                ret_type = code_dir.GetType();
                if (ret_type != rapidjson::kStringType)
                {
                    this->status = OSS_NET_ERROR;
                    return;
                }
                this->code_dir = code_dir.GetString();

                //entry_file
                if (!doc.HasMember("entry_file"))
                {
                    LOG_ERROR << "parse entry_file error. Do not have entry_file";
                    this->status = OSS_NET_ERROR;
                    return;
                }
                rapidjson::Value &entry_file = doc["entry_file"];
                ret_type = entry_file.GetType();
                if (ret_type != rapidjson::kStringType)
                {
                    this->status = OSS_NET_ERROR;
                    return;
                }
                this->entry_file = entry_file.GetString();

                //training_engine
                if (!doc.HasMember("training_engine"))
                {
                    LOG_ERROR << "parse entry_file error. Do not have entry_file";
                    this->status = OSS_NET_ERROR;
                    return;
                }
                rapidjson::Value &training_engine = doc["training_engine"];
                ret_type = training_engine.GetType();
                if (ret_type != rapidjson::kStringType)
                {
                    this->status = OSS_NET_ERROR;
                    return;
                }
                this->training_engine = training_engine.GetString();

                //data_dir
                if (doc.HasMember("data_dir"))
                {
                    rapidjson::Value &data_dir = doc["data_dir"];
                    ret_type = data_dir.GetType();
                    if (ret_type != rapidjson::kStringType)
                    {
                        this->status = OSS_NET_ERROR;
                        return;
                    }
                    this->data_dir = data_dir.GetString();
                }
                //data_dir
                if (doc.HasMember("hyper_parameters"))
                {
                    rapidjson::Value &hyper_parameters = doc["hyper_parameters"];
                    ret_type = hyper_parameters.GetType();
                    if (ret_type != rapidjson::kStringType)
                    {
                        this->status = OSS_NET_ERROR;
                        return;
                    }
                    this->hyper_parameters = hyper_parameters.GetString();
                }
                
            }
            catch (...)
            {
                LOG_ERROR << "container client inspect container resp exception";
            }
        }

    }

}