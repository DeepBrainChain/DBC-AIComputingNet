#ifndef DBC_NETWORK_NIO_LOOP_GROUP_H
#define DBC_NETWORK_NIO_LOOP_GROUP_H

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "common/common.h"

namespace network
{
    class io_service_pool
    {
    public:
        io_service_pool();

        virtual ~io_service_pool();

        ERRCODE init(size_t thread_size);

        ERRCODE start();

        ERRCODE stop();

        ERRCODE exit();

        std::shared_ptr<boost::asio::io_service> get_io_service();

    protected:
        std::mutex m_mutex;
        size_t m_cur_io_service = 0;
        std::vector<std::shared_ptr<boost::asio::io_service>> m_io_services;
        std::vector<std::shared_ptr<boost::asio::io_service::work>> m_works;

        size_t m_thread_size = 1;
        std::vector<std::shared_ptr<std::thread>> m_threads;
        bool m_running = false;
    };
}

#endif
