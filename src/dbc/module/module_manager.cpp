#include "module_manager.h"

int32_t module_manager::init(bpo::variables_map &options)
{
    return init_modules();
}

int32_t module_manager::exit()
{
    return exit_modules();
}

int32_t module_manager::init_modules()
{
    return E_SUCCESS;
}

int32_t module_manager::exit_modules()
{
    for (auto it = m_modules_list.rbegin(); it != m_modules_list.rend(); it++)
    {
        std::shared_ptr<module> module = *it;
        module->stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(DEFAULT_STOP_SLEEP_MILLI_SECONDS));
        module->exit();
    }

    m_modules_list.clear();
    m_modules.clear();

    return E_SUCCESS;
}

std::shared_ptr<module> module_manager::get(const std::string &module_name)
{
    auto it = m_modules.find(module_name);
    if (it == m_modules.end())
    {
        return nullptr;
    }

    return it->second;
}

bool module_manager::add_module(std::string name, std::shared_ptr<module > mdl)
{
    if (nullptr != get(name))
    {
        return false;
    }

    auto p = m_modules.insert(std::make_pair(name, mdl));
    m_modules_list.push_back(mdl);

    return p.second;
}
