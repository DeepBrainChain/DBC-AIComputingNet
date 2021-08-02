#ifndef DBC_SERVICE_PROTO_FILTER_H
#define DBC_SERVICE_PROTO_FILTER_H

#include "util/utils.h"
#include <boost/serialization/singleton.hpp>
#include "util/filter/bloom.h"

#define TIME_OUT_SEC                        180
//#define MAX_NONCE_COUNT             1000000

#define NONCE_ELEMENTS 120000
#define NONCE_FALSE_POSITIVE 0.000001

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
        m_filter_proto_tm->insert(nonce);
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
    rw_lock m_locker;
};

#endif
