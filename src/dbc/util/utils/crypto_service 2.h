#ifndef DBC_CRYPTO_SERVICE_H
#define DBC_CRYPTO_SERVICE_H

#include "common/common.h"
#include <mutex>
#include <boost/program_options.hpp>

namespace bpo = boost::program_options;

class crypto_service
{
public:
    crypto_service() = default;

    ~crypto_service() = default;

    virtual std::string module_name() const { return "crypto service"; }

    virtual int32_t init(bpo::variables_map &options);

    virtual int32_t exit();
};

#endif
