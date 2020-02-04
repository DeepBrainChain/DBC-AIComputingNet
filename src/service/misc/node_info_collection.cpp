/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : node_info_collection.cpp
* description       :
* date              : 2018/6/14
* author            : Jimmy Kuang
**********************************************************************************/

#include <iostream>
#include <fstream>

#include "node_info_collection.h"
#include "node_info_query_sh.h"
#include "log.h"
#include "error.h"
#include "service_topic.h"
#include "server.h"
#include "common/util.h"

#include <boost/program_options.hpp>

#include <sstream>
#include <iostream>
#include <boost/property_tree/json_parser.hpp>

#include <boost/filesystem.hpp>
#include "container_client.h"

namespace bpo = boost::program_options;

namespace service
{
    namespace misc
    {

        static const std::string basic_attrs[] = {
            "version",
            "startup_time",
            "state"
        };

        static const std::string debug_cmds[] = {
            "docker ps",
            "nvidia-smi"
            "container_size"
            "runnning"
        };

        static const std::string dynamic_attrs[] = {
            "gpu_usage",
            "cpu_usage",
            "mem_usage",
            "image",
            "gpu_state",
            "active_task",
            "wallet"
        };

        static const std::string static_attrs[]={
            "network_dl",
            "network_ul",
            "os",
            "ip",
            "cpu",
            "gpu",
            "disk",
            "mem"
        };


        std::vector<std::string> node_info_collection::get_all_attributes()
        {
            std::vector<std::string> v;
            for(auto& k: static_attrs)
            {
                v.push_back(k);
            }

            for(auto& k: dynamic_attrs)
            {
                if(k != "image" && k != "active_task")
                    v.push_back(k);
            }

            v.push_back("state");
            v.push_back("version");

//            for(auto& k: basic_attrs)
//            {
//                v.push_back(k);
//            }

            return v;
        }


        bool is_linux_os()
        {
#ifdef __linux__
            return true;
#endif

            return false;
        }

        enum {
            LINE_SIZE = 2048,
            MAX_KEY_LEN = 1280
        };


        bash_interface::bash_interface()
        {

        }


        bool bash_interface::init(std::string fn, std::string text)
        {
            m_fn = fn;

            if(!is_linux_os())
            {
                return false;
            }

            try
            {
                std::remove(m_fn.c_str());

                std::ofstream qf (m_fn);
                if (!qf.is_open())
                {
                    return false;
                }

                qf << text;
                qf.close();
            }
            catch (...)
            {
                return false;
            }

            return true;
        }

        std::string bash_interface::run(std::string k)
        {
#ifdef __linux__
            std::string cmd = "/bin/bash "+ m_fn + " " + k;
            FILE *proc = popen(cmd.c_str(), "r");
            if (proc != NULL)
            {
                char line[LINE_SIZE];
                std::string result;

                while (fgets(line, LINE_SIZE, proc))
                    result += line;
                pclose(proc);

                matrix::core::string_util::trim(result);
                return result;
            }
#endif
            return "N/A";
        }


        void node_info_collection::generate_node_static_info(std::string path)
        {
#ifdef __linux__
            std::string cmd = "/bin/bash "+ path + "/tool/node_info/node_info.sh" ;
            FILE *proc = popen(cmd.c_str(), "r");
            if (proc != NULL)
            {
                char line[LINE_SIZE];
                std::string result;

                while (fgets(line, LINE_SIZE, proc))
                    result += line;
                pclose(proc);

                LOG_INFO << result;
            }
#endif
        }


        node_info_collection::node_info_collection()
        {
            m_kvs.clear();

            for(auto& k: dynamic_attrs)
            {
                m_kvs[k]="N/A";
            }

            for(auto& k: static_attrs)
            {
                m_kvs[k]="N/A";
            }

            for(auto& k: basic_attrs)
            {
                m_kvs[k]="N/A";
            }

            for(auto& k: debug_cmds)
            {
                m_kvs[k]="N/A";
            }

        }


        int32_t node_info_collection::init(std::string fn)
        {
            if(!is_linux_os())
            {
                LOG_DEBUG << "skip node info collection for none linux os" ;
                return E_SUCCESS;
            }


            if (!m_shell.init(fn, bash_interface_str))
            {
                LOG_ERROR << "fail to init shell interface for computing node!";
                return E_FILE_FAILURE;
            }

            // version
            std::string ver = STR_VER(CORE_VERSION);
            auto s = matrix::core::string_util::remove_leading_zero(ver.substr(2, 2)) + "."
                     + matrix::core::string_util::remove_leading_zero(ver.substr(4, 2)) + "."
                     + matrix::core::string_util::remove_leading_zero(ver.substr(6, 2)) + "."
                     + matrix::core::string_util::remove_leading_zero(ver.substr(8, 2));
            m_kvs["version"] = s;

            // startup time
            set_node_startup_time();

            // read more node info from conf
            std::string node_info_file_path = env_manager::get_home_path().generic_string() + "/.dbc_node_info.conf";
            read_node_static_info(node_info_file_path);

            return E_SUCCESS;
        }


        void node_info_collection::refresh()
        {
            if(!is_linux_os())
            {
                return;
            }

            // reload all dynamic info from
            m_kvs["gpu_usage"] = m_shell.run("gpu_usage");
            m_kvs["cpu_usage"] = m_shell.run("cpu_usage");
            m_kvs["mem_usage"] = m_shell.run("mem_usage");
            m_kvs["image"] = m_shell.run("image");

            {
                std::string s;
                for (auto & k : CONF_MANAGER->get_wallets())
                {
                    if(s.empty())
                    {
                        s="[";
                    }
                    else
                    {
                        s+=",";
                    }

                    s+="\""+k+"\"";
                }

                s+="]";

                m_kvs["wallet"] = s;
            }


            //async invoke
            auto req = std::make_shared<service::get_task_queue_size_req_msg>();

            auto msg = std::dynamic_pointer_cast<message>(req);
            TOPIC_MANAGER->publish<int32_t>(typeid(service::get_task_queue_size_req_msg).name(), msg);
        }

        int32_t node_info_collection::read_node_static_info(std::string fn)
        {
            LOG_DEBUG << fn;


            if (false == boost::filesystem::exists(fn) || false == boost::filesystem::is_regular_file(fn))
            {
                generate_node_static_info(env_manager::get_home_path().generic_string());
            }

            bpo::options_description opts("node info options");
            for(auto& k: static_attrs)
            {
                opts.add_options()(k.c_str(),bpo::value<std::string>(), "");
            }

            bpo::variables_map vm;
            try
            {
                std::ifstream conf_ifs(fn);
                bpo::store(bpo::parse_config_file(conf_ifs, opts, true), vm);
                bpo::notify(vm);

                for(auto& k: static_attrs){
                    if(vm.count(k))
                    {
                        m_kvs[k]=vm[k].as<std::string>();
                    }
                }
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "parse node info conf error: " << e.what();
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }


        time_t node_info_collection::get_node_startup_time()
        {
            auto v = m_kvs["startup_time"];
            time_t s = 0;

            try
            {
                s = std::stoi(v);
            }
            catch (...)
            {
                //skip invalid timestamp
            }

            return s;
        }

        void node_info_collection::set_node_startup_time()
        {
            time_t t = std::time(nullptr);
            if ( t != (std::time_t)(-1))
            {
                m_kvs["startup_time"] = std::to_string(t);
            }

        }


        std::string node_info_collection::pretty_state(std::string v)
        {
            if (v == "0" || v == "N/A")
            {
                return "idle";
            }
            else if ( v == "-1" )
            {
                return "idle(*)";
            }
            else
            {
                return "busy("+v+")";
            }

            return v;
        }

        std::string node_info_collection::get(std::string k)
        {
          //  LOG_INFO << "come in  " ;
            std::string v = "";
            if( k.length() > MAX_KEY_LEN )
            {
                return "keyword overlength";
            }

            if (!m_kvs.count(k) && k.find("container_size")== string::npos && k.find("running")== string::npos)
            {
                return "unknown keyword";
            }


            v = m_kvs[k];
            if ( k == "state")
            {
                return pretty_state(v);
            }

            if(k.find("container_size")!= string::npos){

                LOG_INFO << "come in  get container " ;
                std::vector<std::string> list;
                string_util::split(k, "@", list);

                return get_container(list[1]);
            }

            if(k.compare("running")==0){

                LOG_INFO << "come in  running container " ;
                std::vector<std::string> list;


                return get_running_container();
            }

            // support debug commands
            for(auto& each: debug_cmds)
            {
                if(each == k)
                {

                    return m_shell.run(k);
                }
            }


            return v;
        }

        std::string node_info_collection::get_container(std::string container_name){

            std::shared_ptr<container_client> m_container_client = nullptr;
            std::string m_container_ip=DEFAULT_LOCAL_IP;
            uint16_t m_container_port=(uint16_t)std::stoi(DEFAULT_CONTAINER_LISTEN_PORT);
            m_container_client=std::make_shared<container_client>(m_container_ip, m_container_port);
            return m_container_client->get_container(container_name);
        }


        std::string node_info_collection::get_running_container(){

            std::shared_ptr<container_client> m_container_client = nullptr;
            std::string m_container_ip=DEFAULT_LOCAL_IP;
            uint16_t m_container_port=(uint16_t)std::stoi(DEFAULT_CONTAINER_LISTEN_PORT);
            m_container_client=std::make_shared<container_client>(m_container_ip, m_container_port);
            return m_container_client->get_running_container();
        }


        std::string node_info_collection::get_gpu_usage_in_total()
        {
            //example,
            //gpu_usage:
            //gpu: 10 %
            //mem: 1 %

            // 'kvs': {   u'gpu_usage': u'gpu: 10 %\nmem: 1 %\ngpu: 0 %\nmem: 0 %\n'}

            std::string v = get("gpu_usage");
            if (v.empty() || v == std::string("N/A"))
            {
                return "N/A";
            }

            std::vector<std::string> vec;
            string_util::split(v, "\n", vec);

            std::string start_delim = "gpu: ";
            std::string stop_delim = " %";
            int a = 0;
            int n = 0;

            for (auto& s : vec)
            {
                size_t pos1 = 0;
                size_t pos2 = 0;

                pos1 = s.find(start_delim);
                if (pos1 == std::string::npos) continue;

                pos1 += start_delim.length();

                pos2 = s.find(" %");
                if (pos2 == std::string::npos) continue;

                if (pos1 >= pos2) continue;

                std::string u = s.substr(pos1, pos2-pos1);

                try
                {
                    a += std::stoi(u);
                    n++;
                }
                catch (...)
                {
                    //skip line with invalid digit
                }
            }

            if (n)
            {
                return std::to_string(a/n) + " %";
            }

            return "0 %";
        }

        void node_info_collection::set(std::string k, std::string v)
        {
            m_kvs[k] = v;
        }

        std::string node_info_collection::get_gpu_short_desc()
        {
            std::string raw = m_kvs["gpu"];

            //gpu={"num":"4", "driver":"410.66", "cuda":"10.0", "p2p":"ok", "gpus":[{"id":"0", "type":"GeForceGTX1080Ti", ...

            std::string rt;
            std::stringstream ss;
            ss << raw;
            boost::property_tree::ptree pt;

            try
            {
                boost::property_tree::read_json(ss, pt);

                std::string num = pt.get<std::string>("num");
                int32_t n = atoi(num.c_str());
                if (n > 0)
                {

                    std::map<std::string, int> gpu_map;
                    for(boost::property_tree::ptree::value_type &v:  pt.get_child("gpus"))
                    {
                        boost::property_tree::ptree const &gpu = v.second;
                        auto type = gpu.get<std::string>("type");
                        if (gpu_map.count(type))
                        {
                            gpu_map[type] += 1;
                        }
                        else
                        {
                            gpu_map[type] = 1;
                        }
                    }

                    for(auto& kv: gpu_map)
                    {
                        if (!rt.empty()) rt+=" ";

                        rt += std::to_string(kv.second) + std::string(" * ") +  kv.first;
                    }
                }
            }
            catch (std::exception& e)
            {
                LOG_ERROR << e.what() << endl;
            }

            LOG_DEBUG << "gpu: " << rt;

            return rt;

        }

    }

}