#ifndef DBC_SERVICE_CONTEXT_H
#define DBC_SERVICE_CONTEXT_H

#include <boost/program_options/variables_map.hpp>

using namespace boost::program_options;

class service_context
{
public:
    variables_map & get_args() { return m_args; }

    void add(std::string arg_name, variable_value val) { m_args.insert({ arg_name, val }); }

protected:
    variables_map m_args;           //put variable args into context
};

#endif
