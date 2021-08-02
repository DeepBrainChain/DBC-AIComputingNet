#ifndef DBC_CONF_VALIDATOR_H
#define DBC_CONF_VALIDATOR_H

#include <boost/program_options.hpp>

using namespace boost::program_options;

class conf_validator
{
public:
    virtual bool validate(const variable_value &val) = 0;
};

#endif
