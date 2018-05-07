/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºconnection_manager.cpp
* description    £ºconnection manager as controller class for dbc core connection
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#include "connection_manager.h"
#include <boost/exception/all.hpp>
#include "log.h"
#include "channel.h"


using namespace matrix::core;

namespace matrix
{
    namespace core
    {

        connection_manager::connection_manager() 
            : m_worker_group(new nio_loop_group())
            , m_acceptor_group(new nio_loop_group())
            , m_connector_group(new nio_loop_group())
        {
        }

        int32_t connection_manager::init(bpo::variables_map &options)
        {
            int32_t ret = E_SUCCESS;

            //acceptor group
            ret = m_acceptor_group->init(DEFAULT_ACCEPTOR_THREAD_COUNT);
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "connection manager init acceptor group failed";
                return ret;
            }

            //worker group
            ret = m_worker_group->init(DEFAULT_WORKER_THREAD_COUNT);
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "connection manager init worker group failed";
                m_acceptor_group->exit();
                return ret;
            }

            //connector group
            m_connector_group->init(DEFAULT_CONNECTOR_THREAD_COUNT);
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "connection manager init connector group failed";
                m_acceptor_group->exit();
                m_worker_group->exit();

                return ret;
            }

            return E_SUCCESS;
        }

        int32_t connection_manager::start()
        {
            int32_t ret = start_io_services();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "connection manager start all io services failed";
                return ret;
            }

            return E_SUCCESS;
        }

        int32_t connection_manager::stop()
        {
            //stop server listening
            LOG_DEBUG << "connection manager stop all server listening...";
            stop_all_listen();

            //stop client connect
            LOG_DEBUG << "connection manager stop all client connecting...";
            stop_all_connect();

            LOG_DEBUG << "connection manager stop all tcp channels...";
            stop_all_channel();

            //stop io service
            LOG_DEBUG << "connection manager stop all io services...";
            stop_io_services();

            return E_SUCCESS;
        }

        int32_t connection_manager::exit()
        {
            LOG_DEBUG << "connection manager exit all io services";

            m_acceptor_group->exit();
            m_worker_group->exit();
            m_connector_group->exit();

            write_lock_guard<rw_lock> lock(m_lock);
            m_acceptors.clear();
            m_connectors.clear();
            m_channels.clear();
            return E_SUCCESS;
        }

        int32_t connection_manager::start_io_services()
        {
            int32_t ret = E_SUCCESS;

            //acceptor group
            LOG_DEBUG << "connection manager start acceptor group";
            ret = m_acceptor_group->start();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "connection manager start acceptor io services failed and exit";
                return ret;
            }

            //worker group
            LOG_DEBUG << "connection manager start worker group";
            ret = m_worker_group->start();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "connection manager start woker io services failed and exit";
                m_acceptor_group->stop();
                return ret;
            }

            //connector group
            LOG_DEBUG << "connection manager start connector group";
            ret = m_connector_group->start();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "connection manager start connector io services failed and exit";
                m_acceptor_group->stop();
                m_worker_group->stop();
                return ret;
            }

            return E_SUCCESS;
        }

        int32_t connection_manager::stop_io_services()
        {
            LOG_DEBUG << "connection manager stop acceptor group";
            m_acceptor_group->stop();

            LOG_DEBUG << "connection manager stop worker group";
            m_worker_group->stop();

            LOG_DEBUG << "connection manager stop connector group";
            m_connector_group->stop();

            return E_SUCCESS;
        }

         int32_t connection_manager::stop_all_listen()
        {
            read_lock_guard<rw_lock> lock(m_lock_accp);
            for (auto it = m_acceptors.begin(); it != m_acceptors.end(); it++)
            {
                LOG_DEBUG << "connection manager stop listening at port: " << (*it)->get_endpoint().port();
                (*it)->stop();
            }

            return E_SUCCESS;
         }

        int32_t connection_manager::stop_all_connect()
        {
            read_lock_guard<rw_lock> lock(m_lock_conn);
            for (auto it = m_connectors.begin(); it != m_connectors.end(); it++)
            {
                LOG_DEBUG << "connection manager stop all connect at addr: " << (*it)->get_connect_addr().address() << " port: " << (*it)->get_connect_addr().port();
                (*it)->stop();
            }

            return E_SUCCESS;
        }

        int32_t connection_manager::stop_all_channel()
        {
            std::shared_ptr<matrix::core::channel> channel;
            while(true)
            {
                {
                    read_lock_guard<rw_lock> lock(m_lock_chnl);
                    if (m_channels.begin() != m_channels.end())
                    {
                        channel = m_channels.begin()->second;
                    }
                }
                if (channel)
                {
                    channel->stop();
                    channel.reset();
                }
                else
                {
                    break;
                }
            }

            //while (m_channels.begin() == m_channels.end())
            //{
            //    LOG_DEBUG << "connection manager stop tcp channel at  " << m_channels.begin()->second->id().to_string();
            //    m_channels.begin()->second->stop();
            //}

            return E_SUCCESS;
        }

        int32_t connection_manager::start_listen(tcp::endpoint ep, handler_create_functor func)
        {
            LOG_DEBUG << "connection manager start listening at ip: " << ep.address().to_string() << " at port: " << ep.port();

            try
            {
                std::shared_ptr<io_service> ios(m_acceptor_group->get_io_service());
                std::shared_ptr<tcp_acceptor> acceptor(new tcp_acceptor(ios, m_worker_group, ep, func));
                assert(acceptor != nullptr);

                int32_t ret = acceptor->start();
                if (E_SUCCESS != ret)
                {
                    LOG_ERROR << "connection manager start listen failed at port: " << ep.port();
                    return ret;
                }

                write_lock_guard<rw_lock> lock(m_lock_accp);
                m_acceptors.push_back(acceptor);
            }
            catch (const std::exception &e)
            {
                LOG_ERROR << "connection_manager start listen exception error: " << e.what();
                return E_SUCCESS;
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "connection_manager start listen exception error: " << diagnostic_information(e);
                return E_SUCCESS;               //if formal release version, here should be return ERROR!!!
            }
            catch (...)
            {
                LOG_ERROR << "connection_manager start listen exception error!";
                return E_SUCCESS;               //if formal release version, here should be return ERROR!!!
            }

            return E_SUCCESS;
        }

        int32_t connection_manager::stop_listen(tcp::endpoint ep)
        {
            write_lock_guard<rw_lock> lock(m_lock_accp);
            for (auto it = m_acceptors.begin(); it != m_acceptors.end(); it++)
            {
                if (ep != (*it)->get_endpoint())
                {
                    continue;
                }

                //stop
                LOG_DEBUG << "connection manager stop listening at port: " << ep.port();
                (*it)->stop();

                //erase
                m_acceptors.erase(it);
                return E_SUCCESS;
            }

            return E_SUCCESS;
        }

        int32_t connection_manager::start_connect(tcp::endpoint connect_addr, handler_create_functor func)
        {
            LOG_DEBUG << "connection manager start connect at addr: " << connect_addr.address().to_string() << " " << connect_addr.port();

            try
            {
                std::shared_ptr<tcp_connector> connector = std::make_shared<tcp_connector>(m_connector_group, m_worker_group, connect_addr, func);
                assert(connector != nullptr);

                //start
                int32_t ret = connector->start();
                if (E_SUCCESS != ret)
                {
                    LOG_ERROR << "connection manager start connect failed at addr: " << connect_addr.address().to_string() << " " << connect_addr.port();
                    return ret;
                }

                write_lock_guard<rw_lock> lock(m_lock_conn);
                m_connectors.push_back(connector);
            }
            catch (const std::exception &e)
            {
                LOG_ERROR << "connection_manager start connect exception error: " << e.what();
                return E_SUCCESS;
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "connection_manager start connect exception error: " << diagnostic_information(e);
                return E_DEFAULT;
            }
            catch (...)
            {
                LOG_ERROR << "connection_manager start connect exception error!";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t connection_manager::stop_connect(tcp::endpoint connect_addr)
        {
            write_lock_guard<rw_lock> lock(m_lock_conn);
            for (auto it = m_connectors.begin(); it != m_connectors.end(); it++)
            {
                if (connect_addr != (*it)->get_connect_addr())
                {
                    continue;
                }

                //stop
                LOG_DEBUG << "connection manager stop connect at addr: " << connect_addr.address().to_string() << " " << connect_addr.port();
                (*it)->stop();

                //erase
                m_connectors.erase(it);
                return E_SUCCESS;
            }

            return E_SUCCESS;
        }

        int32_t connection_manager::add_channel(socket_id sid, shared_ptr<channel> channel)
        {
            write_lock_guard<rw_lock> lock(m_lock_chnl);

            LOG_DEBUG << "channel add channel begin use count " << channel.use_count() << channel->id().to_string();

            std::pair<std::map<socket_id, shared_ptr<matrix::core::channel>, cmp_key>::iterator, bool> ret = m_channels.insert(make_pair(sid, channel));
            if (!ret.second)
            {
                LOG_ERROR << "connection manager add channel error, maybe duplicated " << sid.to_string();
                return E_DEFAULT;
            }
            else
            {
                LOG_DEBUG << "connection manager add channel successfully " << sid.to_string();
                LOG_DEBUG << "channel add channel end use count " << channel.use_count() << channel->id().to_string();

                return E_SUCCESS;
            }
        }

        void connection_manager::remove_channel(socket_id sid)
        {
            write_lock_guard<rw_lock> lock(m_lock_chnl);

            //log use count
            shared_ptr<channel> ch = m_channels.find(sid)->second;
            LOG_DEBUG << "channel remove_channel begin use count " << ch.use_count() << ch->id().to_string();

            m_channels.erase(sid);
            LOG_DEBUG << "connection manager remove channel successfully " << sid.to_string();

            LOG_DEBUG << "channel remove_channel end use count " << ch.use_count() << ch->id().to_string();
        }

		std::shared_ptr<channel> connection_manager::get_channel(socket_id sid)
		{
			read_lock_guard<rw_lock> lock(m_lock_chnl);
			auto it = m_channels.find(sid);
			if (it != m_channels.end())
			{
				return it->second;
			}

			return nullptr;
		}

        int32_t connection_manager::send_message(socket_id sid, std::shared_ptr<message> msg)
        {
			read_lock_guard<rw_lock> lock(m_lock_chnl);
            auto it = m_channels.find(sid);
            if (it == m_channels.end())
            {
                LOG_ERROR << "connection manager send message failed for not found " << sid.to_string();
                return E_DEFAULT;
            }

            LOG_DEBUG << "connection manager send message to socket, " << sid.to_string() << ", message name: " << msg->get_name();
            return it->second->write(msg);
        }

		void connection_manager::broadcast_message(std::shared_ptr<message> msg)
		{
			read_lock_guard<rw_lock> lock(m_lock_chnl);
			auto it = m_channels.begin();
			for (; it != m_channels.end(); ++it)
			{
				LOG_DEBUG << "connection manager send message to socket, " << it->first.to_string() << ", message name: " << msg->get_name();
				it->second->write(msg);
			}			
		}

    }

}

