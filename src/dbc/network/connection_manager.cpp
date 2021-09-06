#include "connection_manager.h"
#include <boost/exception/all.hpp>
#include "log/log.h"
#include "tcp_socket_channel.h"
#include "util/utils.h"
#include "timer/timer_def.h"
#include <algorithm>
#include "service_module/service_message_id.h"
#include "server/server.h"

namespace dbc
{
    namespace network
    {
        connection_manager::connection_manager()
        {
            m_worker_group = std::make_shared<nio_loop_group>();
            m_acceptor_group = std::make_shared<nio_loop_group>();
            m_connector_group = std::make_shared<nio_loop_group>();
        }

        connection_manager::~connection_manager() {
            stop_all_listen();
            stop_all_connect();
            stop_all_channel();
            stop_all_recycle_channel();
            stop_io_services();
            exit_io_services();

            {
                write_lock_guard<rw_lock> lock(m_lock_accp);
                m_acceptors.clear();
            }

            {
                write_lock_guard<rw_lock> lock(m_lock_conn);
                m_connectors.clear();
            }

            remove_timers();
        }

        int32_t connection_manager::init(bpo::variables_map &options)
        {
            service_module::init(options);

            //init io services
            int32_t ret = init_io_services();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "connection manager init thread group failed";
                return ret;
            }

            //start io services
            ret = start_io_services();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "connection manager start all io services failed";
                return ret;
            }

            ret = load_max_connect(options);
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "connection manager load max_connect failed";
                return ret;
            }

            return E_SUCCESS;
        }

        int32_t connection_manager::load_max_connect(bpo::variables_map &options)
        {
            m_max_connect = options.count("max_connect") ? options["max_connect"].as<int32_t>()
                    : conf_manager::instance().get_max_connect();

            if (m_max_connect < 0)
            {
                LOG_ERROR << "max_connect config error.";
                return E_DEFAULT;
            }

            // Trim requested connection counts, to fit into system limitations
            m_max_connect = (std::max)((std::min)(m_max_connect, (int32_t)(FD_SETSIZE - 1 - MIN_CORE_FILEDESCRIPTORS - MAX_ADDNODE_CONNECTIONS)), 0);

            int32_t nFD = RaiseFileDescriptorLimit(m_max_connect + MIN_CORE_FILEDESCRIPTORS + MAX_ADDNODE_CONNECTIONS);
            if (nFD < MIN_CORE_FILEDESCRIPTORS)
            {
                LOG_ERROR << "Not enough file descriptors available.";
                return E_DEFAULT;
            }

            m_max_connect = (std::min)(nFD - MIN_CORE_FILEDESCRIPTORS - MAX_ADDNODE_CONNECTIONS, m_max_connect);
            m_max_connect = (std::max)(m_max_connect, MAX_OUTBOUND_CONNECTIONS);
            LOG_DEBUG << "max_connect: " <<m_max_connect;
            return E_SUCCESS;
        }

        void connection_manager::init_subscription()
        {
            topic_manager::instance().subscribe(TCP_CHANNEL_ERROR, [this](std::shared_ptr<message> &msg) { send(msg);});
        }

        void connection_manager::init_invoker()
        {
            invoker_type invoker;

            //tcp channel error
            invoker = std::bind(&connection_manager::on_tcp_channel_error, this, std::placeholders::_1);
            m_invokers.insert({ TCP_CHANNEL_ERROR, { invoker } });
        }

        void connection_manager::init_timer()
        {
            m_timer_invokers[TIMER_NAME_CHANNEL_RECYCLE] = std::bind(&connection_manager::on_recycle_timer, this, std::placeholders::_1);
            m_channel_recycle_timer = add_timer(TIMER_NAME_CHANNEL_RECYCLE, TIMER_INTERVAL_CHANNEL_RECYCLE, ULLONG_MAX, DEFAULT_STRING);
            assert(INVALID_TIMER_ID != m_channel_recycle_timer);
        }

        int32_t connection_manager::init_io_services()
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

        int32_t connection_manager::exit_io_services()
        {
            LOG_INFO << "connection manager exit acceptor group";
            m_acceptor_group->exit();

            LOG_INFO << "connection manager exit worker group";
            m_worker_group->exit();

            LOG_INFO << "connection manager exit connector group";
            m_connector_group->exit();

            return E_SUCCESS;
        }

        int32_t connection_manager::start_io_services()
        {
            int32_t ret = E_SUCCESS;

            //acceptor group
            LOG_INFO << "connection manager start acceptor group";
            ret = m_acceptor_group->start();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "connection manager start acceptor io services failed and exit";
                return ret;
            }

            //worker group
            LOG_INFO << "connection manager start worker group";
            ret = m_worker_group->start();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "connection manager start woker io services failed and exit";
                m_acceptor_group->stop();
                return ret;
            }

            //connector group
            LOG_INFO << "connection manager start connector group";
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
            try
            {
                LOG_INFO << "connection manager stop acceptor group";
                m_acceptor_group->stop();

                LOG_INFO << "connection manager stop worker group";
                m_worker_group->stop();

                LOG_INFO << "connection manager stop connector group";
                m_connector_group->stop();

            }
            catch(const boost::exception & e)
            {
                LOG_ERROR << "connection_manager stop_io_services error:" << diagnostic_information(e);
            }

            return E_SUCCESS;
        }

         int32_t connection_manager::stop_all_listen()
        {
            read_lock_guard<rw_lock> lock(m_lock_accp);
            for (auto it = m_acceptors.begin(); it != m_acceptors.end(); it++)
            {
                LOG_INFO << "connection manager stop listening at port: " << (*it)->get_endpoint().port();
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
            std::shared_ptr<tcp_socket_channel> ch = nullptr; 

            write_lock_guard<rw_lock> lock(m_lock_chnl);
            for (auto it = m_channels.begin(); it != m_channels.end(); it++)
            {
                ch = std::dynamic_pointer_cast<tcp_socket_channel>(it->second);

                if (ch && !ch->is_stopped())
                {
                    ch->stop();
                    ch.reset();
                    }
                }

            m_channels.clear();

            return E_SUCCESS;
                }

        int32_t connection_manager::stop_all_recycle_channel()
                {
            std::shared_ptr<tcp_socket_channel> ch = nullptr;

            write_lock_guard<rw_lock> lock(m_lock_recycle);
            for (auto it = m_recycle_channels.begin(); it != m_recycle_channels.end(); it++)
            {
                ch = std::dynamic_pointer_cast<tcp_socket_channel>(it->second);

                if (ch && !ch->is_stopped())
                {
                    ch->stop();
                    ch.reset();
                }
            }

            m_recycle_channels.clear();

            return E_SUCCESS;
        }

        int32_t connection_manager::start_listen(tcp::endpoint ep, handler_create_functor func)
        {
            LOG_INFO << "connection manager start listening at ip: " << ep.address().to_string() << " at port: " << ep.port();

            try
            {
                std::weak_ptr<io_service> ios(m_acceptor_group->get_io_service());
                std::shared_ptr<tcp_acceptor> acceptor = std::make_shared<tcp_acceptor>(ios, m_worker_group, ep, func);
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
                return E_DEFAULT;
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "connection_manager start listen exception error: " << diagnostic_information(e);
                return E_DEFAULT;
            }
            catch (...)
            {
                LOG_ERROR << "connection_manager start listen exception error!";
                return E_DEFAULT;
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
                LOG_INFO << "connection manager stop listening at port: " << ep.port();
                (*it)->stop();

                //erase
                m_acceptors.erase(it);
                return E_SUCCESS;
            }

            return E_SUCCESS;
        }

        int32_t connection_manager::start_connect(tcp::endpoint connect_addr, handler_create_functor func)
        {
            LOG_DEBUG << "connection manager start connect to addr: " << connect_addr.address().to_string() << " " << connect_addr.port();

            try
            {
                std::shared_ptr<tcp_connector> connector = std::make_shared<tcp_connector>(m_connector_group, m_worker_group, connect_addr, func);
                assert(connector != nullptr);

                //start
                int32_t ret = connector->start();//retry twice
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

        int32_t connection_manager::release_connector(socket_id sid)
        {
            write_lock_guard<rw_lock> lock(m_lock_conn);
            for (auto it = m_connectors.begin(); it != m_connectors.end(); it++)
            {
                if (sid != (*it)->get_socket_id())
                {
                    continue;
                }

                //stop
                LOG_DEBUG << "connection manager release connector of sid: " << sid.to_string();
                (*it)->stop();

                //erase
                m_connectors.erase(it);
                return E_SUCCESS;
            }

            return E_NOT_FOUND;
        }

        int32_t connection_manager::add_channel(socket_id sid, std::shared_ptr<channel> channel)
        {
            write_lock_guard<rw_lock> lock(m_lock_chnl);
            if (check_over_max_connect(sid.get_type()) != true)
            {
                LOG_DEBUG << "add channel failed. over max connect. " << sid.to_string();
                return E_DEFAULT;
            }

            LOG_DEBUG << "channel add channel begin use count " << channel.use_count() << channel->id().to_string();

            std::pair<std::map<socket_id, std::shared_ptr<dbc::network::channel>, cmp_key>::iterator, bool> ret = m_channels.insert(make_pair(sid, channel));
            if (!ret.second)
            {
                LOG_ERROR << "connection manager add channel error, maybe duplicated " << sid.to_string();
                return E_DEFAULT;
            }
            else
            {
                LOG_DEBUG << "connection manager add channel successfully " << sid.to_string();

                return E_SUCCESS;
            }
        }

        void connection_manager::remove_channel(socket_id sid)
        {
            write_lock_guard<rw_lock> lock(m_lock_chnl);

            auto it = m_channels.find(sid);
            if (it == m_channels.end())
            {
                LOG_ERROR << "connection manager remove_channel failed for not found channel." << sid.to_string();
                return;
            }
            
            //log use count
            std::shared_ptr<channel> ch = it->second;
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
                LOG_ERROR << "connection manager send message failed: channel not found " << sid.to_string();
                return E_DEFAULT;
            }

            LOG_DEBUG << "connection manager send message to socket, " << sid.to_string() << ", message name: " << msg->get_name();
            return it->second->write(msg);
        }

		int32_t connection_manager::broadcast_message(std::shared_ptr<message> msg, socket_id id /*= socket_id()*/)
		{
            
            if (msg->content->header.nonce.length() == 0)
            {
                LOG_INFO << "nonce is null. not broadcast. " << msg->get_name();
                return E_NONCE;
            }

            if (! have_active_channel())
            {
                LOG_INFO << "connection manager broadcast message error, no active channel";
                return E_INACTIVE_CHANNEL;
            }

            {
                read_lock_guard<rw_lock> lock(m_lock_chnl);
                auto it = m_channels.begin();
                for (; it != m_channels.end(); ++it)
                {
                    if (id.get_id() != 0)
                    {
                        if (it->first == id)
                            continue;
                    }

                    //not login success or stopped, continue
                    if (!it->second->is_channel_ready())
                    {
                        LOG_INFO << "connection manager broadcast message, but peer socket id: " << it->first.get_id()
                                  << " not logined.";
                        continue;
                    }


                    it->second->write(msg);
                }
                LOG_INFO << "connection manager send message to socket, " << it->first.to_string()
                         << ", message name: " << msg->get_name();
            }

            return E_SUCCESS;
		}

        std::shared_ptr<channel> connection_manager::find_fast_path(std::vector<std::string>& path)
        {
            int i = 0;
            bool is_next_node_found = false;
            std::shared_ptr<channel> c = nullptr;
            for (auto& node_id: path)
            {
                i++;
                auto it = m_channels.begin();
                for (; it != m_channels.end(); ++it)
                {
                    //not login success or stopped, continue
                    if (!it->second->is_channel_ready())
                    {
                        continue;
                    }

                    auto tcp_ch = std::dynamic_pointer_cast<tcp_socket_channel>(it->second);
                    if (tcp_ch && tcp_ch->get_remote_node_id() == node_id)
                    {
                        LOG_INFO << "connection manager send message to socket, " << it->first.to_string();
                        is_next_node_found = true;
                        c = it->second;
                        break;
                    }
                }

                if (is_next_node_found) break;

            }


            if (is_next_node_found)
            {
                uint32_t nodes_to_be_removed = path.size() - i + 1 ;

                for (uint32_t i = 0; i < nodes_to_be_removed; i++)
                {
                    path.pop_back();
                }
            }

            return c;
        }

        bool connection_manager::have_active_channel()
        {
            if (m_channels.size() < 1)
            {
                return false;
            }

            read_lock_guard<rw_lock> lock(m_lock_chnl);
            auto it = m_channels.begin();
            for (; it != m_channels.end(); ++it)
            {
                if (it->second->is_channel_ready())
                {
                    return true;
                }
            }
            return false;
        }

        bool connection_manager::send_resp_message(std::shared_ptr<message> msg, socket_id id)
        {
            if (msg->content->header.nonce.length() == 0)
            {
                LOG_DEBUG << "nonce is null, drop it. " << msg->get_name();
                return false;
            }

            {
                read_lock_guard<rw_lock> lock(m_lock_chnl);

                auto &path = msg->content->header.path;

                auto c = find_fast_path(path);

                if (c != nullptr)
                {
                    LOG_DEBUG << "unicast msg " << msg->get_name();
                    c->write(msg);
                }
            }

            return true;
        }

        int32_t connection_manager::get_connect_num()
        {
            int32_t num = 0;
            auto it = m_channels.begin();
            for (; it != m_channels.end(); ++it)
            {
                if ((it->second)->get_state() != CHANNEL_STOPPED)
                {
                    ++num;
                }
            }
            return num;
        }

        int32_t connection_manager::get_out_connect_num()
        {
            int32_t num = 0;
            auto it = m_channels.begin();
            for (; it != m_channels.end(); ++it)
            {
                if ((it->first).get_type() == CLIENT_SOCKET && (it->second)->get_state() != CHANNEL_STOPPED)
                {
                    ++num;
                }
            }
            return num;
        }

        void connection_manager::on_tcp_channel_error(const std::shared_ptr<message> &msg)
        {
            if (nullptr == msg)
                return;

            socket_id  sid = msg->header.src_sid;
            LOG_DEBUG << "connection manager received tcp channel error:" << sid.to_string();

            stop_channel(sid);
        }
        
        int32_t connection_manager::get_in_connect_num()
        {
            int32_t num = 0;
            auto it = m_channels.begin();
            for (; it != m_channels.end(); ++it)
            {
                if ((it->first).get_type() == SERVER_SOCKET && (it->second)->get_state() != CHANNEL_STOPPED)
                {
                    ++num;
                }
            }
            return num;
        }

        bool connection_manager::check_over_max_connect(socket_type st)
        {

            int32_t num = 0;
            if (st == SERVER_SOCKET)
            {
                num = get_in_connect_num();
                int32_t n_max_in = m_max_connect - MAX_OUTBOUND_CONNECTIONS;
                if (num > n_max_in)
                {
                    return false;
                }
            }

            /*if (st == CLIENT_SOCKET)
            {
                num = get_in_connect_num();

                if (num > MAX_OUTBOUND_CONNECTIONS)
                {
                    return false;
                }
            }*/

            return true;
        }

        void connection_manager::on_recycle_timer(const std::shared_ptr<core_timer>& timer)
        {
            std::queue<socket_id> goodbye_channels;

            write_lock_guard<rw_lock> lock(m_lock_recycle);
            LOG_DEBUG << "connection manager recycle channels count: " << m_recycle_channels.size();

            for (auto it = m_recycle_channels.begin(); it != m_recycle_channels.end(); it++)
            {
                auto ch = std::dynamic_pointer_cast<tcp_socket_channel>(it->second);
                if (nullptr == ch)
                {
                    goodbye_channels.push(it->first);
                    continue;
                }

                if (false == ch->is_stopped())
                {
                    LOG_DEBUG << "connection manager stop tcp socket channel" << ch->id().to_string();
                    ch->stop();
                }
                else
                {
                    LOG_DEBUG << "connection manager tcp socket channel use count: " << ch.use_count() << ch->id().to_string();

                    //delay release 
                    if (ch.use_count() <= MIN_RELEASE_CHANNEL_USE_COUNT)                //it means only auto ch and m_recycle_channels owns this channel
                    {
                        goodbye_channels.push(it->first);
                        LOG_DEBUG << "connection manager add tcp socket channel to goodbye channels, use count: " << ch.use_count() << ch->id().to_string();
                    }
                }
            }

            while (!goodbye_channels.empty())
            {
                //front
                socket_id sid = goodbye_channels.front();
                goodbye_channels.pop();

                LOG_DEBUG << "connection manager erase tcp socket channel from recycle channels:" << sid.to_string();
                m_recycle_channels.erase(sid);
            }
        }

        int32_t connection_manager::stop_channel(socket_id sid)
        {
            std::shared_ptr<tcp_socket_channel> ch = nullptr;

            {
                write_lock_guard<rw_lock> lock(m_lock_chnl);

                //get tcp socket channel
                auto it = m_channels.find(sid);
                if (it == m_channels.end())
                {
                    LOG_ERROR << "connection manager on tcp channel error but not found" << sid.to_string();
                    return E_DEFAULT;
                }

                ch = std::dynamic_pointer_cast<tcp_socket_channel>(it->second);
                if (nullptr == ch)
                {
                    return E_DEFAULT;
                }

                //remove channel
                m_channels.erase(sid);
                LOG_DEBUG << "connection manager on tcp channel error, erase tcp socket channel" << sid.to_string();

                //stop it directly
                if (!ch->is_stopped())
                {
                    LOG_DEBUG << "connection manager stop tcp socket channel" << ch->id().to_string();
                    ch->stop();
                }
            }

            {
                //add recycle channel
                write_lock_guard<rw_lock> lock(m_lock_recycle);

                auto it = m_recycle_channels.find(sid);
                if (it != m_recycle_channels.end())         //already in recycle channels
                {
                    LOG_ERROR << "connection manager add channel to recycle channels but found exists" << sid.to_string();
                    return E_SUCCESS;
                }

                m_recycle_channels[sid] = ch;
                LOG_DEBUG << "connection manager on tcp channel error, add recycle tcp socket channel" << sid.to_string();
            }

            return E_SUCCESS;
        }

        void connection_manager::remove_timers()
        {
            if (INVALID_TIMER_ID != m_channel_recycle_timer)
            {
                remove_timer(m_channel_recycle_timer);
                m_channel_recycle_timer = INVALID_TIMER_ID;
            }
        }

        void connection_manager::set_proto_capacity(socket_id sid, std::string c)
        {
            write_lock_guard<rw_lock> lock(m_lock_chnl);

            //get tcp socket channel
            auto it = m_channels.find(sid);
            if (it == m_channels.end())
            {
                LOG_ERROR << "connection manager on tcp channel error but not found" << sid.to_string();
                return;
            }

            std::shared_ptr<tcp_socket_channel> ch = std::dynamic_pointer_cast<tcp_socket_channel>(it->second);
            if (nullptr == ch)
            {
                return;
            }

            ch->set_proto_capacity(c);

        }
    }
}
