/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   container_message.h
* description    :   container message definition
* date                  :   2018.04.07
* author            :   Bruce Feng
**********************************************************************************/

#include "bill_message.h"
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include "error/en.h"

namespace matrix
{
    namespace core
    {

        std::string auth_task_req::to_string()
        {
            rapidjson::Document document;
            rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

            rapidjson::Value root(rapidjson::kObjectType);

            root.AddMember("mining_node_id", STRING_REF(mining_node_id), allocator);
            root.AddMember("ai_user_node_id", STRING_REF(ai_user_node_id), allocator);
            root.AddMember("task_id", STRING_REF(task_id), allocator);
            root.AddMember("start_time", STRING_REF(start_time), allocator);
            root.AddMember("task_state", STRING_REF(task_state), allocator);
            root.AddMember("end_time", STRING_REF(end_time), allocator);
            root.AddMember("sign_type", STRING_REF(task_state), allocator);
            root.AddMember("sign", STRING_REF(end_time), allocator);

            std::shared_ptr<rapidjson::StringBuffer> buffer = std::make_shared<rapidjson::StringBuffer>();
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(*buffer);
            root.Accept(writer);

            return buffer->GetString();
        }

        void auth_task_resp::from_string(const std::string & buf)
        {
            try
            {
                rapidjson::Document doc;
                //doc.Parse<0>(buf.c_str());              //left to later not all fields set
                if (doc.Parse<0>(buf.c_str()).HasParseError())
                {
                    LOG_ERROR << "parse bill_resp file error:" << GetParseError_En(doc.GetParseError());
                    return;
                }
                //message
                if (!doc.HasMember("status"))
                {
                    LOG_ERROR << "parse bill_resp file error. Do not have status";
                    return;
                }
                rapidjson::Value &status = doc["status"];
                this->status = status.GetInt();

                //contract_state
                if (!doc.HasMember("contract_state"))
                {
                    LOG_ERROR << "parse bill_resp file error. Do not have status";
                    return;
                }
                rapidjson::Value &contract_state = doc["contract_state"];
                this->contract_state = contract_state.GetString();

                //report_cycle
                if (doc.HasMember("report_cycle"))
                {
                    rapidjson::Value &report_cycle = doc["report_cycle"];
                    this->report_cycle = report_cycle.GetInt();
                }
            }
            catch (...)
            {
                LOG_ERROR << "container client inspect container resp exception";
            }
        }

    }

}