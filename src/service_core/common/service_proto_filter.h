#pragma once
#include <map>
#include <boost/serialization/singleton.hpp>
#include "rw_lock.h"

#define TIME_OUT_SEC                        180
#define MAX_NONCE_COUNT             1000000

using namespace matrix::core;

namespace matrix
{
    namespace service_core
    {
        class service_proto_filter : public boost::serialization::singleton<service_proto_filter>
        {
        public:

            bool insert_nonce(const std::string & nonce)
            {
                //return directly
                if (nonce.empty())
                {
                    return false;
                }

                time_t cur = time(nullptr);
                write_lock_guard<rw_lock> lock(m_locker);

                m_map_proto_tm[nonce] = cur;
                return true;
            }

            bool check_dup(const std::string & nonce)
            {
                //return directly
                if (nonce.empty())
                {
                    return false;
                }

                time_t cur = time(nullptr);
                write_lock_guard<rw_lock> lock(m_locker);

                auto it = m_map_proto_tm.find(nonce);
                if (it != m_map_proto_tm.end())
                {
                    if (it->second < cur - TIME_OUT_SEC)        //TIME_OUT_SEC before
                    {
                        it->second = cur;
                        return false;
                    }
                    else
                    {
                        it->second = cur;
                        return true;
                    }
                }
                else
                {
                    //ddos attack?
                    if (m_map_proto_tm.size() >= MAX_NONCE_COUNT)
                    {
                        LOG_ERROR << "message nonce is too many, nonce: " << nonce;
                        return true;
                    }

                    m_map_proto_tm[nonce] = cur;
                }
                
                return false;
            }

            void regular_clean()
            {
                write_lock_guard<rw_lock> lock(m_locker);
                for (auto it = m_map_proto_tm.begin(); it != m_map_proto_tm.end(); )
                {
                    if (it->second < time(nullptr) - TIME_OUT_SEC)
                    {
                        m_map_proto_tm.erase(it++);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

        private:
            std::map<std::string, time_t> m_map_proto_tm;
            matrix::core::rw_lock m_locker;
        };

    }
}

