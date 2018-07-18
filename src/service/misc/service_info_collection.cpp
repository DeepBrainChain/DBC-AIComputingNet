/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : service_info_collection.cpp
* description       : 
* date              : 2018/6/19
* author            : Jimmy Kuang
**********************************************************************************/

#include "service_info_collection.h"
#include "server.h"
#include <stdlib.h>


namespace service
{
    namespace misc
    {

        service_info_collection::service_info_collection()
        {

        }

        /**
         * init
         */
        void service_info_collection::init(std::string own_node_id)
        {
            m_id_2_info.clear();
            m_change.clear();
            m_own_node_id = own_node_id;
        }

        /**
         * clear changes receivd in previous period
         */
        void service_info_collection::reset_change_set()
        {
            m_change.clear();
        }


        /**
         * add one more node into cache
         * @param id
         * @param s
         */
        void service_info_collection::add(std::string id, node_service_info s)
        {
            //std::unique_lock<std::mutex> lock(m_mutex);

            const std::string AI_SERVICE = "ai_training";
            if (std::find(s.service_list.begin(), s.service_list.end(), AI_SERVICE) != s.service_list.end())
            {
                if ( 0 == m_id_2_info.count(id))
                {
                    m_id_2_info[id] = s;

                    m_id_list.push_back(id);
                }
                else
                {
                    auto time_a = s.time_stamp;
                    auto time_b = m_id_2_info[id].time_stamp;
                    if ( time_a > time_b)
                    {
                        m_id_2_info[id] = s;
                    }
                }
            }
        }

        /**
         * return service info from cache
         * @return
         */
        service_info_map& service_info_collection::get()
        {
            //std::unique_lock<std::mutex> lock(m_mutex);

            return m_id_2_info;
        }


        service_info_map& service_info_collection::get_change_set()
        {
            m_change.clear();

            int max_change_set_size = m_id_list.size() < 32 ? m_id_list.size():32;

            for(int i = 0; i < max_change_set_size; i++)
            {
                auto id = m_id_list.front();
                m_id_list.pop_front();
                m_id_list.push_back(id);

                m_change[id] = m_id_2_info[id];
            }

            return m_change;
        }


        /**
         * add a set of nodes info into cache
         * @param m
         */
        void service_info_collection::add(service_info_map m)
        {
            for (auto & item: m)
            {
                if (m_id_2_info.size() > MAX_STORED_SERVICE_INFO_NUM ){
                    LOG_WARNING << "service_info_collection store is full";
                    return;
                }

                add(item.first, item.second);
            }
        }

        /**
         * update time stamp of own node in cache
         * @param own_node_id
         */
        void service_info_collection::update_own_node_time_stamp(std::string own_node_id)
        {
            //std::unique_lock<std::mutex> lock(m_mutex);

            if ( m_id_2_info.count(own_node_id))
            {
                auto t = std::time(nullptr);
                if ( t != (std::time_t)(-1))
                {
                    m_id_2_info[own_node_id].__set_time_stamp(t);
                }
            }

        }

        void service_info_collection::update(std::string node_id, std::string k, std::string v)
        {
            //std::unique_lock<std::mutex> lock(m_mutex);

            if ( m_id_2_info.count(node_id))
            {
                m_id_2_info[node_id].kvs[k] = v;
            }
        }

        /**
         * remove nodes from cache that does not updated for quite a long time
         */
        void service_info_collection::remove_unlived_nodes(int32_t time_in_second)
        {
            //std::unique_lock<std::mutex> lock(m_mutex);

            auto now = std::time(nullptr);
            if ( now == (std::time_t)(-1))
            {
                LOG_ERROR << "std::time() fail";
                return;
            }

            time_t time_threshold = now - time_in_second;

            for (auto it = m_id_2_info.cbegin(); it != m_id_2_info.cend();)
            {
                if (it->second.time_stamp < time_threshold)
                {
                    auto k = it->first;
                    m_id_2_info.erase(it++);
                    m_id_list.erase( std::remove( m_id_list.begin(), m_id_list.end(), k ), m_id_list.end() );
                }
                else
                {
                    ++it;
                }
            }
        }

        int32_t service_info_collection::size()
        {
            //std::unique_lock<std::mutex> lock(m_mutex);
            return m_id_2_info.size();
        }
    }

}