#ifndef DBC_NETWORK_TCP_ACCEPTOR_H
#define DBC_NETWORK_TCP_ACCEPTOR_H

#include <memory>
#include <boost/asio.hpp>
#include "utils/io_service_pool.h"
#include "channel/channel.h"
#include "log/log.h"
#include "channel/handler_create_functor.h"

using namespace boost::asio::ip;

namespace network
{
    class tcp_acceptor : public std::enable_shared_from_this<tcp_acceptor>, boost::noncopyable
    {
    public:
        tcp_acceptor(std::weak_ptr<io_service> io_service, std::shared_ptr<io_service_pool> worker_group,
            tcp::endpoint endpoint, handler_create_functor func);

        virtual ~tcp_acceptor() = default;

        virtual int32_t start();

        virtual int32_t stop();

        ip::tcp::endpoint get_endpoint() const { return m_endpoint; }

    protected:
        virtual int32_t start_accept();

        virtual int32_t on_accept(std::shared_ptr<channel> ch, const boost::system::error_code& error);

    protected:
        ip::tcp::endpoint m_endpoint;

        std::weak_ptr<io_service> m_io_service;
        std::shared_ptr<io_service_pool> m_worker_group;

        tcp::acceptor m_acceptor;
        handler_create_functor m_handler_create_func;
    };
}

#endif
