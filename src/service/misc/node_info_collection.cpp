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
            LINE_SIZE = 1024,
            MAX_KEY_LEN = 128
        };



        node_info_collection::node_info_collection()
        {
            m_query_sh_file_name = ".node_info_query.sh";

            m_node_2_services.clear();
            m_kvs.clear();


            const char* ATTRS[9] = {
            "gpu",
            "cpu",
            "mem",
            "disk",
            "gpu_usage",
            "cpu_usage",
            "mem_usage",
            "state",
            "image"
            };

            int num_of_attrs = sizeof(ATTRS)/sizeof(char*);

            for(int i=0; i< num_of_attrs; i++)
            {
                auto k = ATTRS[i];
                m_keys.push_back(k);
                m_kvs[k] = "N/A";
            }
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

                return result;
            }
#endif
            return "N/A";
        }

        /**
         * update hardware usage of dbc own node
         */
        void node_info_collection::refresh()
        {
            //std::unique_lock<std::mutex> lock(m_mutex);

            if(!is_linux_os())
            {
                return;
            }

            m_kvs["gpu_usage"] = query("gpu_usage");
            m_kvs["cpu_usage"] = query("cpu_usage");
            m_kvs["mem_usage"] = query("mem_usage");
            m_kvs["image"] = query("image");

            //async invoke
            auto req = std::make_shared<service::get_task_queue_size_req_msg>();

            auto msg = std::dynamic_pointer_cast<message>(req);
            TOPIC_MANAGER->publish<int32_t>(typeid(service::get_task_queue_size_req_msg).name(), msg);
        }

        /**
         * initial node info collection, e.g. generate query shell script.
         * @return error code
         */
        int32_t node_info_collection::init()
        {
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

            return E_SUCCESS;
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
                    else
                    {
                        v = "busy("+v+")";
                    }
                }
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

        /**
         * set k,v
         * @param k
         * @param v
         */
        void node_info_collection::set(std::string k, uint32_t v)
        {
            //std::unique_lock<std::mutex> lock(m_mutex);
            m_kvs[k] = std::to_string(v);
        }

    }

}