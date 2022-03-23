#include "io_service_pool.h"

namespace network
{
    io_service_pool::io_service_pool() {

    }

    io_service_pool::~io_service_pool()
    {
        this->exit();
    }

    ERRCODE io_service_pool::init(size_t thread_size)
    {
        m_thread_size = thread_size;
        for (size_t i = 0; i < thread_size; i++)
        {
            std::shared_ptr<boost::asio::io_service> ioservice = std::make_shared<boost::asio::io_service>();
            m_io_services.push_back(ioservice);
            std::shared_ptr<boost::asio::io_service::work> iowork = std::make_shared<boost::asio::io_service::work>(*ioservice.get());
            m_works.push_back(iowork);
        }

        return ERR_SUCCESS;
    }

    ERRCODE io_service_pool::start()
    {
        if (!m_running) {
            m_running = true;

            for (size_t i = 0; i < m_thread_size; i++)
            {
                std::shared_ptr<std::thread> thr(new std::thread(boost::bind(&boost::asio::io_service::run, m_io_services[i])));
                m_threads.push_back(thr);
            }
        }

        return ERR_SUCCESS;
    }

    ERRCODE io_service_pool::stop()
    {
        if (m_running) {
            m_running = false;

            for (size_t i = 0; i < m_thread_size; i++)
            {
                m_io_services[i]->stop();
            }

            for (size_t i = 0; i < m_thread_size; i++) {
                if (m_threads[i]->joinable()) {
                    m_threads[i]->join();
                }
            }
            m_threads.clear();
        }

        return ERR_SUCCESS;
    }

    ERRCODE io_service_pool::exit()
    {
        this->stop();
        m_works.clear();
        m_io_services.clear();
        
        return ERR_SUCCESS;
    }

    std::shared_ptr<boost::asio::io_service> io_service_pool::get_io_service()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        size_t idx = (m_cur_io_service + 1) % m_thread_size;
        return m_io_services[idx];
    }
}
