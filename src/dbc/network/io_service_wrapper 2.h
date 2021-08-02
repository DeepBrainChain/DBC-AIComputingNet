#ifndef DBC_NETWORK_IO_SERVICE_WRAPPER_H
#define DBC_NETWORK_IO_SERVICE_WRAPPER_H

#include <memory>
#include <boost/asio.hpp>

using namespace boost::asio;

namespace dbc
{
    namespace network
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

#endif
