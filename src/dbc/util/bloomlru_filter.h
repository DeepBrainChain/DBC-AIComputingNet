#ifndef DBC_SERVICE_PROTO_FILTER_H
#define DBC_SERVICE_PROTO_FILTER_H

#include "util/utils.h"
#include "util/filter/bloom.h"
#include "lru.hpp"

class bloomlru_filter
{
public:
    bloomlru_filter(unsigned int elements = 120000, double false_positive = 0.000001) {
        m_bloom = new CRollingBloomFilter(elements, false_positive);
        m_lru = new lru::Cache<std::string, int32_t, std::mutex>(elements, 0);
    }

    virtual ~bloomlru_filter() {
        delete m_bloom;
        delete m_lru;
    }

    void insert(const std::string& str) {
        if (str.empty()) {
            return;
        }

        m_bloom->insert(str);
        m_lru->insert(str, 1);
    }

    bool contains(const std::string& str)
    {
        if (str.empty()) {
            return false;
        }

        if (!m_bloom->contains(str)) {
            insert(str);
            return false;
        }
        else
        {
            if (m_lru->contains(str)) {
                return true;
            }
            else {
                insert(str);
                return false;
            }
        }
    }

private:
    CRollingBloomFilter* m_bloom = nullptr;
    lru::Cache<std::string, int32_t, std::mutex>* m_lru = nullptr;
};

#endif
