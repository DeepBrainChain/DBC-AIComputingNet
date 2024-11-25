#include "deeplink_lan_service.h"

#include <boost/asio.hpp>

#include "config/conf_manager.h"
#include "deeplink_session.h"
#include "server/server.h"

using boost::asio::ip::tcp;

ERRCODE deeplink_lan_service::init() {
    service_module::init();

    if (Server::NodeType == NODE_TYPE::BareMetalNode) {
        try {
            m_io_service_pool = std::make_shared<network::io_service_pool>();
            ERRCODE err = m_io_service_pool->init(1);
            if (ERR_SUCCESS != err) {
                LOG_ERROR << "init deeplink lan io service failed";
                return err;
            }

            tcp::endpoint ept(
                boost::asio::ip::address::from_string(
                    ConfManager::instance().GetDeeplinkListenIp()),
                ConfManager::instance().GetDeeplinkListenPort());
            m_acceptor = std::make_shared<tcp::acceptor>(
                *m_io_service_pool->get_io_service().get(), ept);
            do_accept();

            // m_receiver = std::make_shared<multicast_receiver>(
            //     *m_io_service_pool->get_io_service().get(),
            //     boost::asio::ip::make_address(
            //         ConfManager::instance().GetNetListenIp()),
            //     boost::asio::ip::make_address(
            //         ConfManager::instance().GetMulticastAddress()),
            //     ConfManager::instance().GetMulticastPort());
            // err = m_receiver->start();
            // if (ERR_SUCCESS != err) {
            //     LOG_ERROR << "init multicast receiver failed";
            //     return err;
            // }

            // m_sender = std::make_shared<multicast_sender>(
            //     *m_io_service_pool->get_io_service().get(),
            //     boost::asio::ip::make_address(
            //         ConfManager::instance().GetMulticastAddress()),
            //     ConfManager::instance().GetMulticastPort());

            err = m_io_service_pool->start();
            if (ERR_SUCCESS != err) {
                LOG_ERROR << "deeplink lan service run failed";
                return err;
            }
        } catch (std::exception& e) {
            LOG_ERROR << "init deeplink lan service Exception: " << e.what();
            return -1;
        }
    }

    return ERR_SUCCESS;
}

void deeplink_lan_service::exit() {
    service_module::exit();
    if (Server::NodeType == NODE_TYPE::BareMetalNode) {
        // m_receiver->stop();
        // m_sender->stop();
        m_io_service_pool->stop();
        m_io_service_pool->exit();
    }
}

FResult deeplink_lan_service::get_device_info(const std::string& host,
                                              std::string& device_id,
                                              std::string& password) {
    return m_room.get_device_info(host, device_id, password);
}

FResult deeplink_lan_service::set_device_info(const std::string& host,
                                              const std::string& device_id,
                                              const std::string& password) {
    return m_room.set_device_info(host, device_id, password);
}

void deeplink_lan_service::do_accept() {
    m_acceptor->async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<deeplink_session>(std::move(socket), m_room)
                    ->start();
            }

            do_accept();
        });
}
