#ifndef DBC_NETWORK_NIO_LOOP_GROUP_H
#define DBC_NETWORK_NIO_LOOP_GROUP_H

#include "common/common.h"
#include <thread>
#include <vector>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "io_service_wrapper.h"

#define MAX_LOOP_GROUP_THREAD_SIZE                         64

namespace dbc
{
    namespace network
    {
        class nio_loop_group
        {
        public:

            nio_loop_group() : m_cur_io_service(0) {}

            virtual ~nio_loop_group();

            virtual int32_t init(size_t thread_size);

            virtual int32_t start();

            virtual int32_t stop();

            virtual int32_t exit();

            virtual std::shared_ptr<io_service> get_io_service();

        protected:

            size_t m_thread_size;

            std::mutex m_mutex;

            std::size_t m_cur_io_service;

            std::vector<std::shared_ptr<std::thread>> m_threads;

            std::vector<std::shared_ptr<io_service_wrapper>> m_io_services;
        };
    }
}

#endif
