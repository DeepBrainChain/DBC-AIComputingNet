/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   module_manager.h
* description    :   module manager for all the modules
* date                  :   2018.01.20
* author            :   Bruce Feng
**********************************************************************************/

#ifndef _SERVICE_MODULE_MANAGER_H_
#define _SERVICE_MODULE_MANAGER_H_

#include <unordered_map>
#include <string>
#include "common.h"
#include "module.h"


namespace bpo = boost::program_options;


namespace matrix
{
    namespace core
    {
        
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

        };

    }

}

#endif

