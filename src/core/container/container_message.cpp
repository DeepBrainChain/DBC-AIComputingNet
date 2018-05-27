/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   container_message.h
* description    :   container message definition
* date                  :   2018.04.07
* author            :   Bruce Feng
**********************************************************************************/

#include "container_message.h"
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"

namespace matrix
{
    namespace core
    {

        container_config::container_config()
        {
            tty = false;
            stdin_open = false;
            stdin_once = false;

            memory_limit = 0;
            memory_swap = 0;
            cup_shares = 0;

            attach_stdin = false;
            attach_stdout = false;
            attach_stderr = false;
            network_disabled = false;
            host_config.privileged = false;
            host_config.publish_all_ports = false;
        }

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
            root.AddMember("NetworkDisabled", network_disabled, allocator);
            //root.AddMember("Privileged", privileged, allocator);
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

            //volumes_from
            /*rapidjson::Value json_volumes_from(rapidjson::kArrayType);
            for (auto it = this->volumes_from.begin(); it != this->volumes_from.end(); it++)
            {
                json_volumes_from.PushBack(rapidjson::Value().SetString(it->c_str(), (rapidjson::SizeType)it->length()), allocator);
            }
            root.AddMember("VolumesFrom", json_volumes_from, allocator);*/

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

            //container_host_config
            rapidjson::Value json_host_config(rapidjson::kObjectType);

            //json_host_config: binds
            rapidjson::Value json_binds(rapidjson::kArrayType);
            for (auto it = host_config.binds.begin(); it != host_config.binds.end(); it++)
            {
                json_binds.PushBack(rapidjson::Value().SetString(it->c_str(), (rapidjson::SizeType)it->length()), allocator);
            }
            json_host_config.AddMember("Binds", json_binds, allocator);

            //json_host_config: devices
            rapidjson::Value json_devices(rapidjson::kArrayType);
            for (auto it = host_config.devices.begin(); it != host_config.devices.end(); it++)
            {
                rapidjson::Value json_device(rapidjson::kObjectType);

                json_device.AddMember("PathOnHost", STRING_REF(it->path_on_host), allocator);
                json_device.AddMember("PathInContainer", STRING_REF(it->path_in_container), allocator);
                json_device.AddMember("CgroupPermissions", STRING_REF(it->cgroup_permissions), allocator);

                json_devices.PushBack(json_device.Move(), allocator);
            }
            json_host_config.AddMember("Devices", json_devices, allocator);

            //json_host_config: volume_driver
            json_host_config.AddMember("VolumeDriver", STRING_REF(host_config.volume_driver), allocator);

            //json_host_config: mounts
            rapidjson::Value json_mounts(rapidjson::kArrayType);
            for (auto it = host_config.mounts.begin(); it != host_config.mounts.end(); it++)
            {
                rapidjson::Value json_mount(rapidjson::kObjectType);

                json_mount.AddMember("Type", STRING_REF(it->type), allocator);
                json_mount.AddMember("Name", STRING_REF(it->name), allocator);
                json_mount.AddMember("Source", STRING_REF(it->source), allocator);
                json_mount.AddMember("Destination", STRING_REF(it->destination), allocator);
                json_mount.AddMember("Driver", STRING_REF(it->driver), allocator);
                json_mount.AddMember("Mode", STRING_REF(it->mode), allocator);
                json_mount.AddMember("RW", it->rw, allocator);
                json_mount.AddMember("Propagation", STRING_REF(it->propagation), allocator);

                json_mounts.PushBack(json_mount.Move(), allocator);
            }
            json_host_config.AddMember("Mounts", json_mounts, allocator);

            //json_host_config: links
            rapidjson::Value json_links(rapidjson::kArrayType);
            for (auto it = host_config.links.begin(); it != host_config.links.end(); it++)
            {
                json_links.PushBack(rapidjson::Value().SetString(it->c_str(), (rapidjson::SizeType)it->length()), allocator);
            }
            json_host_config.AddMember("Links", json_links, allocator);

            //json_host_config: port_bindings

            //json_host_config: privileged
            json_host_config.AddMember("Privileged", host_config.privileged, allocator);

            //json_host_config: publish_all_ports
            json_host_config.AddMember("PublishAllPorts", host_config.publish_all_ports, allocator);

            //json_host_config: dns

            //json_host_config: dns_search

            //json_host_config: volumes_from

            root.AddMember("HostConfig", json_host_config, allocator);

            std::shared_ptr<rapidjson::StringBuffer> buffer = std::make_shared<rapidjson::StringBuffer>();
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(*buffer);
            root.Accept(writer);

            return buffer->GetString();
        }

        void container_create_resp::from_string(const std::string & buf)
        {
            try
            {
                //parse resp
                rapidjson::Document doc;
                doc.Parse<0>(buf.c_str());

                //id
                if (false == doc.HasMember("Id"))
                {
                    LOG_ERROR << "container client create container resp has no id field";
                    return;
                }

                rapidjson::Value &id = doc["Id"];
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
            catch (...)
            {
                LOG_ERROR << "container client create container resp exception";
            }

        }

        void container_inspect_response::from_string(const std::string & buf)
        {
            try
            {
                rapidjson::Document doc;
                doc.Parse<0>(buf.c_str());              //left to later not all fields set

                //message
                if (!doc.HasMember("State"))
                {
                    LOG_ERROR << "container client inspect container resp has no id state";
                    return;
                }

                rapidjson::Value &state = doc["State"];

                //running state
                if (state.HasMember("Running"))
                {
                    rapidjson::Value &running = state["Running"];
                    this->state.running = running.GetBool();
                }

                //exit code
                if (state.HasMember("ExitCode"))
                {
                    rapidjson::Value &exit_code = state["ExitCode"];
                    this->state.exit_code = exit_code.GetInt();
                }
            }
            catch (...)
            {
                LOG_ERROR << "container client inspect container resp exception";
            }

        }

    }

}