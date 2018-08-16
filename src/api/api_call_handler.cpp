/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        : api_call_handler.cpp
* description    : api call handler for command line, json rpc
* date                  : 2018.03.27
* author             :  Bruce Feng
**********************************************************************************/

#include "matrix_coder.h"
#include "api_call_handler.h"
#include "matrix_coder.h"
#include "service_message_id.h"


namespace ai
{
    namespace dbc
    {

        void api_call_handler::init_subscription()
        {
            TOPIC_MANAGER->subscribe(typeid(cmd_start_training_resp).name(), [this](std::shared_ptr<cmd_start_training_resp> &rsp) {m_resp = rsp; m_wait->set(); });
            TOPIC_MANAGER->subscribe(typeid(cmd_stop_training_resp).name(), [this](std::shared_ptr<cmd_stop_training_resp> &rsp) {m_resp = rsp; m_wait->set(); });
            TOPIC_MANAGER->subscribe(typeid(cmd_start_multi_training_resp).name(), [this](std::shared_ptr<cmd_start_multi_training_resp> &rsp) {m_resp = rsp; m_wait->set(); });
            TOPIC_MANAGER->subscribe(typeid(cmd_list_training_resp).name(), [this](std::shared_ptr<cmd_list_training_resp> &rsp) {m_resp = rsp; m_wait->set(); });
            TOPIC_MANAGER->subscribe(typeid(cmd_get_peer_nodes_resp).name(), [this](std::shared_ptr<cmd_get_peer_nodes_resp> &rsp) {m_resp = rsp; m_wait->set(); });
            TOPIC_MANAGER->subscribe(typeid(cmd_logs_resp).name(), [this](std::shared_ptr<cmd_logs_resp> &rsp) {m_resp = rsp; m_wait->set(); });
            TOPIC_MANAGER->subscribe(typeid(cmd_show_resp).name(), [this](std::shared_ptr<cmd_show_resp> &rsp) {m_resp = rsp; m_wait->set(); });
            //TOPIC_MANAGER->subscribe(typeid(cmd_clear_resp).name(), [this](std::shared_ptr<cmd_clear_resp> &rsp) {m_resp = rsp; m_wait->set(); });
            TOPIC_MANAGER->subscribe(typeid(cmd_ps_resp).name(), [this](std::shared_ptr<cmd_ps_resp> &rsp) {m_resp = rsp; m_wait->set(); });
        }


        cmd_logs_resp::series cmd_logs_resp::m_series;

        //cmd_logs_resp::cmd_logs_resp()
        //{
        //    no_duplicate = false;
        //}


        /**
         *
         * @param s, one line of log text
         * @return timestamp in string or empty string if no valid timestamp found
         */
        std::string cmd_logs_resp::get_log_date(std::string &s)
        {
            // timestamp pattern: 2018-08-14T13:
            if (s.length() > TIMESTAMP_STR_LENGTH && (s[0] == '2' && s[4] == '-' && s[7] == '-'))
            {
                return s.substr(0, 30);
            }
            else
            {
                return "";
            }
        }

        /**
         *
         * @param s logs text
         * @return  hash value or empty string if no hash found
         */
        std::string cmd_logs_resp::get_training_result_hash_from_log(std::string &s)
        {

            // check if task exec end
            std::string task_completed="end to exec dbc_task.sh";
            if ( s.find(task_completed) == std::string::npos)
            {
                std::cout << "task exec does not complete yet" << std::endl;

                return "";
            }


            //
            std::string prefix = "DIR_HASH:";

            size_t start = s.find(prefix);
            if (start == std::string::npos)
            {
                return "";
            }

            size_t end = s.find("\n", start);
            if (end != std::string::npos)
            {
                auto hash = s.substr(start + prefix.length(), end - start - prefix.length());

                return hash;
            }
            else
            {
                return "";
            }

        }

        void cmd_logs_resp::format_output_segment()
        {
            if (E_SUCCESS != result)
            {
                cout << result_info << endl;
                return;
            }

            LOG_DEBUG << "recv: " << result_info;

            auto it = peer_node_logs.begin();
            for (; it != peer_node_logs.end(); it++)
            {
                cout
                << "****************************************************************************************************"
                << endl;
                cout << "node id: " << it->peer_node_id << endl;
                const char *p = (it->log_content).c_str();
                size_t size = (it->log_content).size();
                for (size_t i = 0; i < size; i++)
                {
                    if (char_filter.find(*p) != char_filter.end())
                    {
                        cout << *p;
                    }
                    p++;
                }
            }
            cout << "\n";
        }

        void cmd_logs_resp::format_output_series()
        {
            if (E_SUCCESS != result)
            {
                cout << result_info << endl;
                return;
            }

            LOG_DEBUG << "recv: " << result_info;

            auto it = peer_node_logs.begin();
            std::string date;
            std::string task_completed="end to exec dbc_task.sh";

            for (; it != peer_node_logs.end(); it++)
            {
                std::string s;
                const char *p = (it->log_content).c_str();
                size_t size = (it->log_content).size();

                for (size_t i = 0; i < size; i++, p++)
                {
                    if (char_filter.find(*p) == char_filter.end())
                    {
                        continue;
                    }

                    s.append(1, *p);
                    if (*p == '\n')
                    {
                        // skip duplicate lines
                        date = get_log_date(s);
                        if (!date.empty() && m_series.last_log_date.compare(date) < 0)
                        {
                            cout << s;
                        }

                        // check if task completed
                        if ( s.find(task_completed) != std::string::npos)
                        {
                            m_series.enable = false;
                        }

                        s.clear();
                    }
                }

                if (!s.empty())
                {
                    // in case no "\n" at the end. for example, progress bar.
                    for (char &c : s)
                    {
                        cout << c;
                    }

                    cout << "\n";

                    s.clear();
                }
            }

            if (!date.empty())
            {
                m_series.last_log_date = date;
            }
        }

        void cmd_logs_resp::format_output()
        {
            // download taining result
            if (sub_op == std::string("result"))
            {
                download_training_result();
                return;
            }

            // logs auto flush
            if (m_series.enable)
            {
                format_output_series();
            }
            else
            {
                format_output_segment();
            }
        }

        /**
         *  extract training result hash from log and then download to local disk.
         */
        void cmd_logs_resp::download_training_result()
        {
            auto n = peer_node_logs.size();

            if ( n == 0 )
            {
                cout << " training result is empty" << endl;
                return;
            }
            else if ( n != 1 )
            {
                cout << " training result is invalid " << endl;
                return;
            }

            auto hash = get_training_result_hash_from_log(peer_node_logs[0].log_content);

            if (hash.empty())
            {
                cout << " not find training result hash " << endl;
                return;
            }

            cout << "  download training result: " << dest_folder << "/training_result_file" << endl;

            std::string cmd_;
            if (dest_folder.empty())
            {
                cmd_ = "ipfs get " + hash;
            }
            else
            {
                cmd_ = "ipfs get " + hash + " -o " + dest_folder;
            }

            cout << cmd_ << endl;
            system(cmd_.c_str());
        }




        cmd_show_resp::cmd_show_resp(): op(OP_SHOW_UNKNOWN),
                         err(""),
                         sort("gpu")
        {

        }

        void cmd_show_resp::error(std::string err_)
        {
            err = err_;
        }

        std::string cmd_show_resp::to_string(std::vector<std::string> in)
        {
            std::string out="";
            for(auto& item: in)
            {
                if(out.length())
                {
                    out += " , ";
                }
                out += item ;
            }

            return out;
        }


        void cmd_show_resp::format_service_list()
        {
            console_printer printer;
            printer(LEFT_ALIGN, 48)(LEFT_ALIGN, 16)(LEFT_ALIGN, 12)(LEFT_ALIGN, 32)(LEFT_ALIGN, 12)(LEFT_ALIGN, 24)(LEFT_ALIGN, 24);

            printer << matrix::core::init << "ID" << "NAME" << "VERSION" << "GPU" <<"STATE" << "SERVICE" << "TIMESTAMP" << matrix::core::endl;


            // order by indicated filed
            std::map<std::string, node_service_info> s_in_order;
            for (auto &it : *id_2_services)
            {
                std::string k;
                if (sort == "name" )
                {
                    k = it.second.name;
                }
                else if (sort == "timestamp")
                {
                    k = it.second.time_stamp;
                }
                else
                {
                    k = it.second.kvs[sort];
                }

                auto v = it.second;
                v.kvs["id"]=it.first;
                s_in_order[k + it.first] = v;  // "k + id" is unique
            }


            for (auto &it : s_in_order)
            {
                std::string ver = it.second.kvs.count("version") ? it.second.kvs["version"] : "N/A";

                std::string gpu = string_util::rtrim(it.second.kvs["gpu"],'\n');

                if (gpu.length() > 31)
                {
                    gpu = gpu.substr(0,31);
                }

                printer << matrix::core::init
                        << it.second.kvs["id"]
                        << it.second.name
                        << ver
                        << gpu
                        << it.second.kvs["state"]
                        << to_string(it.second.service_list)
                        << time_util::time_2_str(it.second.time_stamp)
                        << matrix::core::endl;
            }
            printer << matrix::core::endl;

        }


        void cmd_show_resp::format_node_info()
        {

            auto it = kvs.begin();
            //cout << "node id: " << o_node_id << endl;

            auto count = kvs.size();
            for (; it != kvs.end(); it++)
            {
                //cout << "******************************************************\n";
                cout << "------------------------------------------------------\n";
                if (count)
                {
                    cout << it->first << ":\n";
                }
                cout << it->second << "\n";
                cout << "------------------------------------------------------\n";
                //cout << "******************************************************\n";
            }
            cout << "\n";

        }

        void cmd_show_resp::format_output()
        {
            if (err != "")
            {
                cout << err << endl;
                return;
            }

            if (op == OP_SHOW_SERVICE_LIST)
            {
                format_service_list();
                return;
            }
            else if (op == OP_SHOW_NODE_INFO)
            {
                format_node_info();
                return;
            }
            else
            {

                LOG_ERROR << "unknown op " << op;
            }
        }

    }
}
