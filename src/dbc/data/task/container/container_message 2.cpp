#include "container_message.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"
#include "service/oss/oss_common_def.h"
#include "log/log.h"
#include <boost/format.hpp>

container_config::container_config()
{
    tty = false;
    stdin_open = false;
    stdin_once = false;

    memory_limit = 0;
    //memory_swap = 0;
    cpu_shares = 0;

    attach_stdin = false;
    attach_stdout = false;
    attach_stderr = false;
    network_disabled = false;
    host_config.privileged = true;
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
    //root.AddMember("Memory", memory_limit, allocator);
    //root.AddMember("MemorySwap", memory_swap, allocator);
    //root.AddMember("CpuShares", cup_shares, allocator);
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
    auto it_bind = this->volumes.binds.begin();
    auto it_mode = this->volumes.modes.begin();
    for (auto it = this->volumes.dests.begin(); it != this->volumes.dests.end(); it++)
    {
        rapidjson::Value json_volumes_bind(rapidjson::kObjectType);

        json_volumes_bind.AddMember("bind",rapidjson::Value().SetString(it_bind->c_str(), (rapidjson::SizeType)it_bind->length()), allocator);
        json_volumes_bind.AddMember("mode",rapidjson::Value().SetString(it_mode->c_str(), (rapidjson::SizeType)it_mode->length()), allocator);


        json_volumes.AddMember(rapidjson::Value().SetString(it->c_str(), (rapidjson::SizeType)it->length()), json_volumes_bind, allocator);

        //LOG_INFO << "it_bind："+it_bind;
        //LOG_INFO << "it_bind："+it_bind;

        it_bind++;
        it_mode++;
    }
    root.AddMember("Volumes", json_volumes, allocator);

    //exposed ports
    // e.g. "ExposedPorts":{"22/tcp":{}}
    rapidjson::Value json_exposed_ports(rapidjson::kObjectType);
    for (auto const & it : host_config.port_bindings.ports)
    {
        const container_port &c = it.second;
        std::string k = boost::str(boost::format("%s/%s") %c.port %c.scheme);

        rapidjson::Value empty(rapidjson::kObjectType);
        json_exposed_ports.AddMember(STRING_DUP(k), empty, allocator);
    }

    root.AddMember("ExposedPorts", json_exposed_ports, allocator);

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
  //  json_host_config.AddMember("OomKillDisable", true, allocator);

    //json_host_config: volume_driver
    json_host_config.AddMember("VolumeDriver", STRING_REF(host_config.volume_driver), allocator);

    //json_host_config: mounts
    rapidjson::Value json_mounts(rapidjson::kArrayType);
    for (auto it = host_config.mounts.begin(); it != host_config.mounts.end(); it++)
    {
        rapidjson::Value json_mount(rapidjson::kObjectType);
        json_mount.AddMember("Target", STRING_REF(it->target), allocator);
        json_mount.AddMember("Source", STRING_REF(it->source), allocator);
        json_mount.AddMember("Type", STRING_REF(it->type), allocator);
        json_mount.AddMember("Consistency", STRING_REF(it->consistency), allocator);
        json_mount.AddMember("ReadOnly", it->read_only, allocator);
        //json_mount.AddMember("Destination", STRING_REF(it->destination), allocator);

        //json_mount.AddMember("Driver", STRING_REF(it->driver), allocator);
        //json_mount.AddMember("Mode", STRING_REF(it->mode), allocator);
        //json_mount.AddMember("RW", it->rw, allocator);
        //json_mount.AddMember("Propagation", STRING_REF(it->propagation), allocator);

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
    // e.g. "PortBindings":{"22/tcp":[{"HostIp":"","HostPort":"1022"}]},
    rapidjson::Value json_host_portsbinding(rapidjson::kObjectType);
    for (auto const & it : host_config.port_bindings.ports)
    {
        const container_port& c = it.second;

        std::string k = boost::str(boost::format("%s/%s") %c.port %c.scheme);


        rapidjson::Value ports(rapidjson::kArrayType);

        rapidjson::Value port(rapidjson::kObjectType);
        port.AddMember("HostIp", STRING_REF(c.host_ip), allocator);
        port.AddMember("HostPort", STRING_REF(c.host_port), allocator);

        ports.PushBack(port.Move(), allocator);

        json_host_portsbinding.AddMember(STRING_DUP(k), ports, allocator);
    }

    json_host_config.AddMember("PortBindings",  json_host_portsbinding, allocator);



    //json_host_config: privileged
    json_host_config.AddMember("Privileged", host_config.privileged, allocator);

    //json_host_config: publish_all_ports
    json_host_config.AddMember("PublishAllPorts", host_config.publish_all_ports, allocator);

    //json_host_config: Memory
    json_host_config.AddMember("Memory", host_config.memory, allocator);

    //json_host_config: MemorySwap
    json_host_config.AddMember("MemorySwap", host_config.memory_swap, allocator);

    //json_host_config.AddMember("NanoCPUs", host_config.nano_cpus, allocator);
    json_host_config.AddMember("CpuShares", host_config.cpu_shares, allocator);
    json_host_config.AddMember("CpuPeriod", host_config.cpu_period, allocator);
    json_host_config.AddMember("CpuQuota", host_config.cpu_quota, allocator);

    rapidjson::Value json_restartPolicy(rapidjson::kObjectType);

    json_restartPolicy.AddMember("Name", "unless-stopped", allocator);
    json_restartPolicy.AddMember("MaximumRetryCount", 0, allocator);

    json_host_config.AddMember("RestartPolicy", json_restartPolicy, allocator);

  //  json_host_config.AddMember("DiskQuota",host_config.disk_quota , allocator);


    rapidjson::Value json_storage(rapidjson::kObjectType);
    json_storage.AddMember("size", STRING_REF(host_config.storage), allocator);
    json_host_config.AddMember("StorageOpt",json_storage , allocator);

    json_host_config.AddMember("ShmSize", host_config.share_memory, allocator);

    if(!host_config.runtime.empty())
    {
        json_host_config.AddMember("Runtime",  STRING_REF(host_config.runtime), allocator);
    }

    rapidjson::Value json_ulimts(rapidjson::kArrayType);
    for (auto it = host_config.ulimits.begin(); it != host_config.ulimits.end(); it++)
    {
        rapidjson::Value json_limt(rapidjson::kObjectType);
        json_limt.AddMember("Name", STRING_REF(it->m_name), allocator);
        json_limt.AddMember("Hard", it->m_hard, allocator);
        json_limt.AddMember("Soft", it->m_soft, allocator);
        json_ulimts.PushBack(json_limt.Move(), allocator);
    }

    json_host_config.AddMember("Ulimits", json_ulimts, allocator);

    //json_host_config: dns

    //json_host_config: dns_search

    //json_host_config: volumes_from

    root.AddMember("HostConfig", json_host_config, allocator);

    std::shared_ptr<rapidjson::StringBuffer> buffer = std::make_shared<rapidjson::StringBuffer>();
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(*buffer);
    root.Accept(writer);
    std::string s=buffer->GetString();

    LOG_INFO << "buffer->GetString():"+s;
    return s;
}

std::string update_container_config::update_to_string()
{
    rapidjson::Document document;
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    rapidjson::Value root(rapidjson::kObjectType);


    //env
    rapidjson::Value json_env(rapidjson::kArrayType);
    for (auto it = this->env.begin(); it != this->env.end(); it++)
    {
        json_env.PushBack(rapidjson::Value().SetString(it->c_str(), (rapidjson::SizeType)it->length()), allocator);
    }
    root.AddMember("Env", json_env, allocator);


    //json_host_config: Memory
    root.AddMember("Memory", this->memory, allocator);

    //json_host_config: MemorySwap
    root.AddMember("MemorySwap", this->memory_swap, allocator);

    //json_host_config.AddMember("NanoCPUs", host_config.nano_cpus, allocator);
    root.AddMember("CpuShares", this->cpu_shares, allocator);
    root.AddMember("CpuPeriod", this->cpu_period, allocator);
    root.AddMember("CpuQuota", this->cpu_quota, allocator);

  //  root.AddMember("DiskQuota",this->disk_quota , allocator);
    rapidjson::Value json_storage(rapidjson::kObjectType);
    json_storage.AddMember("size", STRING_REF(this->storage), allocator);
    root.AddMember("StorageOpt",json_storage , allocator);

   // root.AddMember("Gpus",this->gpus , allocator);

    std::shared_ptr<rapidjson::StringBuffer> buffer = std::make_shared<rapidjson::StringBuffer>();
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(*buffer);
    root.Accept(writer);
    std::string s=buffer->GetString();

    LOG_INFO << "buffer->GetString():"+s;
    return s;
}


void container_create_resp::from_string(const std::string & buf)
{
    try
    {
        //parse resp
        rapidjson::Document doc;
        //doc.Parse<0>(buf.c_str());

        if (doc.Parse<0>(buf.c_str()).HasParseError())
        {
            LOG_ERROR << "parse container_create_resp file error:" << GetParseError_En(doc.GetParseError());
            return ;
        }

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
        //doc.Parse<0>(buf.c_str());              //left to later not all fields set
        if (doc.Parse<0>(buf.c_str()).HasParseError())
        {
            LOG_ERROR << "parse container_inspect_response file error:" << GetParseError_En(doc.GetParseError());
            return;
        }

        if (E_SUCCESS != parse_item_string(doc, "Id", this->id))
        {
            return;
        }

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

void docker_info::from_string(const std::string & buf)
{
    try
    {
        rapidjson::Document doc;
        //doc.Parse<0>(buf.c_str());              //left to later not all fields set
        if (doc.Parse<0>(buf.c_str()).HasParseError())
        {
            LOG_ERROR << "parse container_inspect_response file error:" << GetParseError_En(doc.GetParseError());
            return;
        }
        //message
        if (!doc.HasMember("DockerRootDir"))
        {
            LOG_ERROR << "container client inspect container resp has no id state";
            return;
        }

        rapidjson::Value &root_dir = doc["DockerRootDir"];
        if (root_dir.GetType() == rapidjson::kStringType)
        {
            this->root_dir = root_dir.GetString();
        }

        //runtimes
        if (doc.HasMember("Runtimes"))
        {
            rapidjson::Value &json_runtimes = doc["Runtimes"];
            if (json_runtimes.HasMember(RUNTIME_NVIDIA))
            {
                runtimes[RUNTIME_NVIDIA] = RUNTIME_NVIDIA;
            }
        }

    }
    catch (...)
    {
        LOG_ERROR << "container client get docker info exception";
    }
}


void images_info::from_string(const std::string & buf)
{
    try
    {
        rapidjson::Document doc;
        //doc.Parse<0>(buf.c_str());              //left to later not all fields set
        if (doc.Parse<0>(buf.c_str()).HasParseError())
        {
            LOG_ERROR << "parse images list file error:" << GetParseError_En(doc.GetParseError());
            return;
        }


        rapidjson::Value & contents = doc;
        if (contents.IsArray()) {
            for (size_t i = 0; i < contents.Size(); ++i) {
                rapidjson::Value & v = contents[i];
                image_info info;
                assert(v.IsObject());

                info.id=v["Id"].GetString();
                info.size=v["Size"].GetInt64();
                info.virtual_size=v["VirtualSize"].GetInt64();
                info.containers=v["Containers"].GetInt();

                this->list_images_info.push_back(info);
            }
        }



    }
    catch (...)
    {
        LOG_ERROR << "container client get docker info exception";
    }
}
