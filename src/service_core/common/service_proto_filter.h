#pragma once
#include <map>
#include <boost/serialization/singleton.hpp>
#include "rw_lock.h"
#include "bloom.h"

#define TIME_OUT_SEC                        180
//#define MAX_NONCE_COUNT             1000000

#define NONCE_ELEMENTS 120000
#define NONCE_FALSE_POSITIVE 0.000001
using namespace matrix::core;

namespace matrix
{
    namespace service_core
    {
        class service_proto_filter : public boost::serialization::singleton<service_proto_filter>
        {
        public:
            service_proto_filter()
            {
                m_filter_proto_tm = std::make_shared<CRollingBloomFilter>(NONCE_ELEMENTS, NONCE_FALSE_POSITIVE);
            }

            bool insert_nonce(const std::string & nonce)
            {
                //return directly
                if (nonce.empty())
                {
                    return false;
                }

                write_lock_guard<rw_lock> lock(m_locker);
                if (m_nonce_count > MAX_NONCE_COUNT)
                {
                    m_nonce_count = 0;
                    m_filter_proto_tm->reset();
                }
                m_filter_proto_tm->insert(nonce);
                m_nonce_count++;
                return true;
            }

            bool check_dup(const std::string & nonce)
            {
                //return directly
                if (nonce.empty())
                {
                    return false;
                }

                if (m_filter_proto_tm->contains(nonce))
                {
                    return true;
                }
                else
                {
                    insert_nonce(nonce);
                    return false;
                }

                return false;
            }

        private:
            std::shared_ptr<CRollingBloomFilter> m_filter_proto_tm;
            matrix::core::rw_lock m_locker;
            uint64_t m_nonce_count = 0;
            const uint64_t MAX_NONCE_COUNT = NONCE_ELEMENTS*1000;
        };
    }
}

