#pragma once
#include <map>
#include <boost/serialization/singleton.hpp>
#include "rw_lock.h"
#include "service_message.h"
#include "matrix_types.h"
#include "timer_def.h"
#include <boost/pointer_cast.hpp>

#define TIME_OUT_SEC    180

namespace matrix
{
    namespace service_core
    {
        class service_proto_filter : public boost::serialization::singleton<service_proto_filter>
        {
        public:

            bool check_dup(const std::shared_ptr<matrix::core::message>& msg)
            {
                std::shared_ptr<matrix::service_core::msg_header> hdr = boost::reinterpret_pointer_cast<matrix::service_core::msg_header>(msg->content);
                if(hdr)
                {
                    time_t cur = time(nullptr);
                    write_lock_guard<rw_lock> lock(m_locker);
                    auto it = m_map_proto_tm.find(hdr->check_sum);
                    if (it != m_map_proto_tm.end())
                    {
                        if (it->second < cur - TIME_OUT_SEC)
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
                        m_map_proto_tm[hdr->check_sum] = cur;
                    }
                }
                return false;
            }

            void regular_clean()
            {
                write_lock_guard<rw_lock> lock(m_locker);
                for (auto it = m_map_proto_tm.begin(); it != m_map_proto_tm.end(); )
                {
                    if (it->second < time(nullptr) - TIME_OUT_SEC * 2)
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
            std::map<int32_t, time_t> m_map_proto_tm;
            matrix::core::rw_lock m_locker;
        };

    }
}

