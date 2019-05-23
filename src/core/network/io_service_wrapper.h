/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   io_service_wrapper.h
* description      :   wrap io_service into io_service::work
* date             :   2018.01.20
* author           :   Bruce Feng
**********************************************************************************/
#pragma once


#include <memory>
#include <boost/asio.hpp>


using namespace std;
using namespace boost::asio;


namespace matrix
{
    namespace core
    {
        class io_service_wrapper
        {
        public:

            io_service_wrapper()
            {
                m_io_service = std::make_shared<io_service>();
                m_io_service_work = std::make_shared<io_service::work>(*m_io_service);
            }

            ~io_service_wrapper() = default;

            std::shared_ptr<io_service> get_io_sevice() { return m_io_service; }

            void stop() { m_io_service->stop(); }

        protected:

            std::shared_ptr<io_service> m_io_service;

            std::shared_ptr<io_service::work> m_io_service_work;
        };

    }

}
