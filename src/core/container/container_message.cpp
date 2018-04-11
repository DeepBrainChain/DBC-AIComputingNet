/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºcontainer_message.h
* description    £ºcontainer message definition
* date                  : 2018.04.07
* author            £ºBruce Feng
**********************************************************************************/

#include "container_message.h"
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"

namespace matrix
{
    namespace core
    {

        std::string container_config::to_string()
        {
            rapidjson::Document document;
            rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

            rapidjson::Value root(rapidjson::kObjectType);

            root.AddMember("Hostname", STRING_REF(host_name), allocator);
            root.AddMember("user", STRING_REF(user), allocator);
            root.AddMember("Tty", tty, allocator);
            root.AddMember("OpenStdin", stdin_open, allocator);
            root.AddMember("StdinOnce", stdin_once, allocator);
            root.AddMember("Memory", memory_limit, allocator);
            root.AddMember("MemorySwap", memory_swap, allocator);
            root.AddMember("CpuShares", cup_shares, allocator);
            root.AddMember("AttachStdin", attach_stdin, allocator);
            root.AddMember("AttachStdout", attach_stdout, allocator);
            root.AddMember("AttachStderr", attach_stderr, allocator);
            root.AddMember("Image", STRING_REF(image), allocator);
            root.AddMember("VolumesFrom", STRING_REF(volumes_from), allocator);
            root.AddMember("NetworkDisabled", network_disabled, allocator);
            root.AddMember("Privileged", privileged, allocator);
            root.AddMember("WorkingDir", STRING_REF(working_dir), allocator);
            root.AddMember("Domainname", STRING_REF(domain_name), allocator);
            
            //port specs
            rapidjson::Value json_port_specs(rapidjson::kArrayType);
            for (auto it = this->port_specs.begin(); it != this->port_specs.end(); it++)
            {
                json_port_specs.PushBack(rapidjson::Value().SetString(it->c_str(), (rapidjson::SizeType)it->length()), allocator);
            }
            root.AddMember("PortSpecs", json_port_specs, allocator);
            
            //env
            rapidjson::Value json_env(rapidjson::kArrayType);
            for (auto it = this->env.begin(); it != this->env.end(); it++)
            {
                json_env.PushBack(rapidjson::Value().SetString(it->c_str(), (rapidjson::SizeType)it->length()), allocator);
            }
            root.AddMember("Env", json_env, allocator);

            //cmd
            rapidjson::Value json_cmd(rapidjson::kArrayType);
            for (auto it = this->cmd.begin(); it != this->cmd.end(); it++)
            {
                json_cmd.PushBack(rapidjson::Value().SetString(it->c_str(), (rapidjson::SizeType)it->length()), allocator);
            }
            root.AddMember("Cmd", json_cmd, allocator);

            //dns
            rapidjson::Value json_dns(rapidjson::kArrayType);
            for (auto it = this->dns.begin(); it != this->dns.end(); it++)
            {
                json_dns.PushBack(rapidjson::Value().SetString(it->c_str(), (rapidjson::SizeType)it->length()), allocator);
            }
            root.AddMember("Dns", json_dns, allocator);

            //entrypoint
            rapidjson::Value json_entry_point(rapidjson::kArrayType);
            for (auto it = this->entry_point.begin(); it != this->entry_point.end(); it++)
            {
                json_entry_point.PushBack(rapidjson::Value().SetString(it->c_str(), (rapidjson::SizeType)it->length()), allocator);
            }
            root.AddMember("Entrypoint", json_entry_point, allocator);

            //onBuild
            rapidjson::Value json_on_build(rapidjson::kArrayType);
            for (auto it = this->on_build.begin(); it != this->on_build.end(); it++)
            {
                json_on_build.PushBack(rapidjson::Value().SetInt(*it), allocator);
            }
            root.AddMember("OnBuild", json_on_build, allocator);

            //Volumes
            rapidjson::Value json_volumes(rapidjson::kObjectType);
            for (auto it = this->volumes.dests.begin(); it != this->volumes.dests.end(); it++)
            {
                json_volumes.AddMember(rapidjson::Value().SetString(it->c_str(), (rapidjson::SizeType)it->length()), rapidjson::Value().SetString("{}"), allocator);
            }
            root.AddMember("Volumes", json_volumes, allocator);

            std::shared_ptr<rapidjson::StringBuffer> buffer = std::make_shared<rapidjson::StringBuffer>();
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(*buffer);
            root.Accept(writer);

            return buffer->GetString();
        }

        void container_create_resp::from_string(const std::string & buf)
        {
            //parse resp
            rapidjson::Document doc;
            doc.Parse<0>(buf.c_str());

            //id
            if (false == doc.HasMember("id"))
            {
                LOG_ERROR << "container client create container resp has no id field";
            }
            rapidjson::Value &id = doc["id"];
            this->container_id = id.GetString();

            //warinings
            if (doc.HasMember("Warnings"))
            {
                rapidjson::Value &warnings = doc["Warnings"];
                if (warnings.IsArray())
                {
                    for (rapidjson::SizeType i = 0; i < warnings.Size(); i++)
                    {
                        const rapidjson::Value& object = warnings[i];
                        this->warnings.push_back(object.GetString());
                    }
                }
            }

        }

        void container_inspect_response::from_string(const std::string & buf)
        {
            rapidjson::Document doc;
            doc.Parse<0>(buf.c_str());              //left to later not all fields set

            //message
            if (!doc.HasMember("State"))
            {
                return;
            }

            rapidjson::Value &state = doc["State"];
            rapidjson::Value &running = state["Running"];
            rapidjson::Value &exit_code = state["ExitCode"];

            this->state.running = running.GetBool();
            this->state.exit_code = exit_code.GetInt();
        }

    }

}