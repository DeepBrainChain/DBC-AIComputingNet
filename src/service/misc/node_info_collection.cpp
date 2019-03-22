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

namespace service
{
    namespace misc
    {

        bool is_linux_os()
        {
#ifdef __linux__
            return true;
#endif

            return false;
        }

        enum {
            LINE_SIZE = 2048,
            MAX_KEY_LEN = 128
        };


        node_info_collection::node_info_collection()
        {
            m_enable = true;
            m_honest = true;

            m_query_sh_file_name = "" ; //env_manager::get_home_path().generic_string() + "/.node_info_query.sh";
            m_sh_file_hash = std::hash<std::string> {} (node_info_query_sh_str);

            m_node_2_services.clear();

            reset_node_info();
        }

        void node_info_collection::set_query_sh(std::string fn)
        {
            if (fn.empty())
            {
                m_query_sh_file_name = env_manager::get_home_path().generic_string() + "/.node_info_query.sh";
            }
            else
            {
                m_query_sh_file_name = fn;
            }
        }

        void node_info_collection::reset_node_info()
        {
            m_kvs.clear();

            const char* ATTRS[] = {
            "gpu",
            "cpu",
            "mem",
            "disk",
            "gpu_usage",
            "gpu_driver",
            "cpu_usage",
            "mem_usage",
            "state",
            "image",
            "version",
            "startup_time",
            "gpu_state"
            };

            int num_of_attrs = sizeof(ATTRS)/sizeof(char*);

            for(int i=0; i< num_of_attrs; i++)
            {
                auto k = ATTRS[i];
                m_keys.push_back(k);
                m_kvs[k] = "N/A";
            }

            std::string ver = STR_VER(CORE_VERSION);
            auto s = matrix::core::string_util::remove_leading_zero(ver.substr(2, 2)) + "."
                    + matrix::core::string_util::remove_leading_zero(ver.substr(4, 2)) + "."
                    + matrix::core::string_util::remove_leading_zero(ver.substr(6, 2)) + "."
                    + matrix::core::string_util::remove_leading_zero(ver.substr(8, 2));
            m_kvs["version"] = s;
        }

        std::vector<std::string> node_info_collection::get_keys()
        {
            return m_keys;
        }



        #include <sys/stat.h>
        /**
        * Check if a file exists
        * @return true if and only if the file exists, false else
        */
        bool file_exists(const std::string &file)
        {
            struct stat buf;
            return (stat(file.c_str(), &buf) == 0);
        }


        /**
         * create node info query sh file and store in host
         * @param text
         * @return true if file created, otherwise return false
         */
        bool node_info_collection::generate_query_sh_file(std::string text)
        {
            if(!m_enable)
            {
                return false;
            }

            if(!is_linux_os())
            {
                return false;
            }

            try
            {
                if (file_exists(m_query_sh_file_name))
                {
                    int rtn = std::remove(m_query_sh_file_name.c_str());
                    if (rtn)
                    {
                        LOG_ERROR << "fail to remove " << m_query_sh_file_name << " error code: " << rtn;
                        std::cout << "fail to remove " << m_query_sh_file_name << " error code: " << rtn << "\n";
                        std::cout << "please remove this file from shell with proper user account\n";
                        return false;
                    }
                }
            }
            catch (...)
            {
                LOG_ERROR << "fail to remvoe " << m_query_sh_file_name;
                std::cout << "fail to remove " << m_query_sh_file_name << "\n";
                std::cout << "please remove this file from shell with proper user account\n";
                return false;
            }

            std::ofstream qf (m_query_sh_file_name);
            if (!qf.is_open())
            {
                return false;
            }

            qf << text;
            qf.close();
            return true;
        }

        /**
        * query
        * @param k the name of the attribute to be queried
        * @return the attribute value
        */
        std::string node_info_collection::query(std::string k)
        {
#ifdef __linux__
            std::string cmd = "/bin/bash "+ m_query_sh_file_name + " " + k;
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

        bool node_info_collection::check_sh_file(std::string fn)
        {

            std::ifstream t(m_query_sh_file_name);

            if(t)
            {
                std::stringstream buffer;
                buffer << t.rdbuf();

                std::size_t h = std::hash<std::string>{} (buffer.str());

                if (h == m_sh_file_hash)
                {
                    return true;
                }
            }
            return false;
        }

        /**
         * update hardware usage of dbc own node
         */
        void node_info_collection::refresh()
        {
            //std::unique_lock<std::mutex> lock(m_mutex);

            if(!m_enable)
            {
                return;
            }

            if(!is_linux_os())
            {
                return;
            }

            // cheat once then be forbiden forever
            if (m_honest)
            {
                m_honest = check_sh_file(node_info_query_sh_str);
            }

            if(!m_honest)
            {
                LOG_ERROR << "SH file check failed, it is not a honest node!";

                reset_node_info();
                m_kvs["gpu"] = "suspected node";

                return;
            }

            m_kvs["gpu"] = query("gpu");
            m_kvs["gpu_usage"] = query("gpu_usage");
            m_kvs["cpu_usage"] = query("cpu_usage");
            m_kvs["mem_usage"] = query("mem_usage");
            m_kvs["image"] = query("image");
            m_kvs["disk"] = query("disk");

            //async invoke
            auto req = std::make_shared<service::get_task_queue_size_req_msg>();

            auto msg = std::dynamic_pointer_cast<message>(req);
            TOPIC_MANAGER->publish<int32_t>(typeid(service::get_task_queue_size_req_msg).name(), msg);
        }

        /**
         * initial node info collection, e.g. generate query shell script.
         * @return error code
         */
        int32_t node_info_collection::init(bool enable)
        {
            if(!enable)
            {
                m_enable = false;
                return E_SUCCESS;
            }

            if(!is_linux_os())
            {
                LOG_DEBUG << "skip node info collection for none linux os" ;
                return E_SUCCESS;
            }

            if (!generate_query_sh_file(node_info_query_sh_str))
            {
                LOG_ERROR << "generate node info query sh file failed!";
                return E_FILE_FAILURE;
            }

            m_kvs["gpu"] = query("gpu");
            m_kvs["cpu"] = query("cpu");
            m_kvs["mem"] = query("mem");
            m_kvs["disk"] = query("disk");
            m_kvs["image"] = query("image");
            m_kvs["gpu_driver"] = query("gpu_driver");

            set_node_startup_time();

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


        /**
         * to string
         * @return string of one attribute
         */
        std::string node_info_collection::get_one(std::string k)
        {
            std::string v = "";
            if( k.length() <= MAX_KEY_LEN && m_kvs.count(k) )
            {
                v = m_kvs[k];
                if ( k == "state")
                {
                    if (v == "0" || v == "N/A")
                    {
                        v = "idle";
                    }
                    else if ( v == "-1" )
                    {
                        v = "idle(*)";
                    }
                    else
                    {
                        v = "busy("+v+")";
                    }
                }
            }
            else if (k == std::string("nvidia-smi"))
            {
                v = query(k);
            }
            else if (k == std::string("docker ps"))
            {
                v = query(k);
            }
            else
            {
                v = "unknown keyword";
            }

            return v;
        }

        /**
         * get the attribute value
         * @param k the name of attribute
         * @return attribute value
         */
        std::string node_info_collection::get(std::string k)
        {
            //std::unique_lock<std::mutex> lock(m_mutex);
            return get_one(k);
        }


        std::string node_info_collection::get_gpu_usage_in_total()
        {
            //example,
            //gpu_usage:
            //gpu: 10 %
            //mem: 1 %

            // 'kvs': {   u'gpu_usage': u'gpu: 10 %\nmem: 1 %\ngpu: 0 %\nmem: 0 %\n'}

            std::string v = get_one("gpu_usage");
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

        /**
         * set k,v
         * @param k
         * @param v
         */
        void node_info_collection::set(std::string k, std::string v)
        {
            //std::unique_lock<std::mutex> lock(m_mutex);
            m_kvs[k] = v;
        }


        bool node_info_collection::is_honest_node()
        {
            return m_honest;
        }

    }

}