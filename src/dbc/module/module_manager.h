#ifndef DBC_MODULE_MANAGER_H
#define DBC_MODULE_MANAGER_H

#include "util/utils.h"
#include "module.h"

namespace bpo = boost::program_options;

#define DEFAULT_STOP_SLEEP_MILLI_SECONDS        100

class module_manager : public module
{
public:

    module_manager() = default;

    virtual ~module_manager() = default;

    virtual int32_t init(bpo::variables_map &options);

    virtual int32_t exit();

    virtual std::string module_name() const { return module_manager_name; }

    virtual std::shared_ptr<module> get(const std::string &module_name);

    virtual bool add_module(std::string name, std::shared_ptr<module > mdl);

protected:

    //left to derived class
    virtual int32_t init_modules();

    virtual int32_t exit_modules();

protected:

    std::unordered_map<std::string, std::shared_ptr<module>> m_modules;

    std::list<std::shared_ptr<module>> m_modules_list;

};

#endif

