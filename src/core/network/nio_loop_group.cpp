/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºnio_loop_group.cpp
* description    £ºnio thread loop group for network connection
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#include "nio_loop_group.h"
#include "common.h"

namespace matrix
{
    namespace core
    {
        nio_loop_group::~nio_loop_group()
            {
                m_io_services.clear();
                m_threads.clear();
            }

            int32_t nio_loop_group::init(size_t thread_size)
            {
                if (thread_size > MAX_LOOP_GROUP_THREAD_SIZE)
                {
                    LOG_ERROR << "nio loop group init thread size error: " << thread_size;
                    return E_DEFAULT;
                }

                m_thread_size = thread_size;
                for (size_t i = 0; i < m_thread_size; i++)
                {
                    std::shared_ptr<io_service_wrapper> ios(new io_service_wrapper);
                    m_io_services.push_back(ios);
                }

                return E_SUCCESS;
            }

            int32_t nio_loop_group::start()
            {
                try
                {
                    //bind io_service to thread
                    for (size_t i = 0; i < m_io_services.size(); i++)
                    {
                        std::shared_ptr<std::thread> thr(new std::thread(boost::bind(&io_service::run, m_io_services[i]->get_io_sevice())));
                        m_threads.push_back(thr);
                    }
                }
                catch (const std::system_error &e)
                {
                    LOG_ERROR << "nio loop group start create thread failed: " << e.what();
                    return E_DEFAULT;
                }
                catch (...)
                {
                    LOG_ERROR << "nio loop group start create thread failed!";
                    return E_DEFAULT;
                }

                return E_SUCCESS;
            }

            int32_t nio_loop_group::stop()
            {
                //stop io_service
                for (size_t i = 0; i < m_io_services.size(); i++)
                {
                    //exit ios loop cycle
                    m_io_services[i]->stop();
                }

                return E_SUCCESS;
            }

            int32_t nio_loop_group::exit()
            {
                //join thread
                for (size_t i = 0; i < m_io_services.size(); i++)
                {
                    try
                    {
                        m_threads[i]->join();
                    }
                    catch (const std::system_error &e)
                    {
                        LOG_ERROR << "nio_loop_group join thread(" << m_threads[i]->get_id() << ")error: " << e.what();
                        continue;
                    }
                    catch (...)
                    {
                        LOG_ERROR << "nio_loop_group join thread(" << m_threads[i]->get_id() << ")error: unknown";
                        continue;
                    }
                }

                return E_SUCCESS;
            }

            std::shared_ptr<io_service> nio_loop_group::get_io_service()
            {
                std::unique_lock<std::mutex> lock(m_mutex);

                shared_ptr<io_service_wrapper> wrapper(m_io_services[m_cur_io_service++]);
                if (m_cur_io_service == m_io_services.size())
                {
                    m_cur_io_service = 0;
                }

                return wrapper->get_io_sevice();
            }

    }

}