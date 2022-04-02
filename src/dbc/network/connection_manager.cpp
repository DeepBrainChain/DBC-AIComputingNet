#include "connection_manager.h"
#include <boost/exception/all.hpp>
#include "log/log.h"
#include "channel/tcp_socket_channel.h"
#include "util/utils.h"
#include "timer/timer_def.h"
#include <algorithm>
#include "config/conf_manager.h"

namespace network
{
    static int32_t RaiseFileDescriptorLimit(int nMinFD)
    {
        struct rlimit limitFD;
        if (getrlimit(RLIMIT_NOFILE, &limitFD) == 0)
        {
            if (limitFD.rlim_cur < (rlim_t)nMinFD)
            {
                limitFD.rlim_cur = nMinFD;
                if (limitFD.rlim_cur > limitFD.rlim_max) {
                    limitFD.rlim_cur = limitFD.rlim_max;
                }
                setrlimit(RLIMIT_NOFILE, &limitFD);
                getrlimit(RLIMIT_NOFILE, &limitFD);
            }
            return limitFD.rlim_cur;
        }

        return nMinFD;
    }

    ERRCODE connection_manager::init()
    {
        service_module::init();

		m_worker_group = std::make_shared<io_service_pool>();
		m_acceptor_group = std::make_shared<io_service_pool>();
		m_connector_group = std::make_shared<io_service_pool>();
        
        ERRCODE ret = init_io_services();
        if (ERR_SUCCESS != ret) {
            LOG_ERROR << "init thread group failed";
            return ret;
        }
        
        ret = start_io_services();
        if (ERR_SUCCESS != ret) {
            LOG_ERROR << "start io_services failed";
            return ret;
        }

        ret = load_max_connect();
        if (ERR_SUCCESS != ret) {
            LOG_ERROR << "load_max_connect failed";
            return ret;
        }

        return ERR_SUCCESS;
    }

    void connection_manager::init_invoker()
    {
        reg_msg_handle(TCP_CHANNEL_ERROR, CALLBACK_1(connection_manager::on_tcp_channel_error, this));
    }

    void connection_manager::init_timer()
    {
        add_timer(TIMER_NAME_CHANNEL_RECYCLE, TIMER_INTERVAL_CHANNEL_RECYCLE, TIMER_INTERVAL_CHANNEL_RECYCLE, 
            ULLONG_MAX, "", CALLBACK_1(connection_manager::on_recycle_timer, this));
    }

	void connection_manager::exit() {
        service_module::exit();

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
	}

    ERRCODE connection_manager::init_io_services()
    {
        ERRCODE ret = ERR_SUCCESS;
        
        ret = m_acceptor_group->init(DEFAULT_ACCEPTOR_THREAD_COUNT);
        if (ERR_SUCCESS != ret) {
            LOG_ERROR << "init acceptor_group failed";
            return ret;
        }
         
        ret = m_worker_group->init(DEFAULT_WORKER_THREAD_COUNT);
        if (ERR_SUCCESS != ret) {
            LOG_ERROR << "init worker_group failed";
            
            m_acceptor_group->exit();
            return ret;
        }
        
        m_connector_group->init(DEFAULT_CONNECTOR_THREAD_COUNT);
        if (ERR_SUCCESS != ret) {
            LOG_ERROR << "init connector_group failed";

            m_acceptor_group->exit();
            m_worker_group->exit(); 
            return ret;
        }

        return ERR_SUCCESS;
    }

    ERRCODE connection_manager::start_io_services()
    {
        ERRCODE ret = ERR_SUCCESS;

        ret = m_acceptor_group->start();
        if (ERR_SUCCESS != ret) {
            LOG_ERROR << "start acceptor_group failed";
            return ret;
        }
        
        ret = m_worker_group->start();
        if (ERR_SUCCESS != ret) {
            LOG_ERROR << "start woker_group failed";
            m_acceptor_group->stop();
            return ret;
        }
        
        ret = m_connector_group->start();
        if (ERR_SUCCESS != ret) {
            LOG_ERROR << "start connector_group failed";
            m_acceptor_group->stop();
            m_worker_group->stop();
            return ret;
        }

        return ERR_SUCCESS;
    }

    ERRCODE connection_manager::stop_io_services()
    {
        m_acceptor_group->stop();
        m_worker_group->stop();
        m_connector_group->stop();
       
        return ERR_SUCCESS;
    }
    
    ERRCODE connection_manager::exit_io_services()
    {
        m_acceptor_group->exit();
        m_worker_group->exit();
        m_connector_group->exit();

        return ERR_SUCCESS;
    }

    ERRCODE connection_manager::load_max_connect()
    {
        m_max_connect = ConfManager::instance().GetMaxConnectCount();

        if (m_max_connect <= 0) {
            LOG_ERROR << "max_connect config error";
            return E_DEFAULT;
        }

        m_max_connect = (std::max)((std::min)(m_max_connect, 
            (int32_t)(FD_SETSIZE - 1 - MIN_CORE_FILEDESCRIPTORS - MAX_ADDNODE_CONNECTIONS)), 0);

        int32_t nFD = RaiseFileDescriptorLimit(m_max_connect + MIN_CORE_FILEDESCRIPTORS + MAX_ADDNODE_CONNECTIONS);
        if (nFD < MIN_CORE_FILEDESCRIPTORS) {
            LOG_ERROR << "not enough fd available";
            return E_DEFAULT;
        }

        m_max_connect = (std::min)(nFD - MIN_CORE_FILEDESCRIPTORS - MAX_ADDNODE_CONNECTIONS, m_max_connect);
        m_max_connect = (std::max)(m_max_connect, MAX_OUTBOUND_CONNECTIONS);

        return ERR_SUCCESS;
    }

    ERRCODE connection_manager::stop_all_listen()
    {
        read_lock_guard<rw_lock> lock(m_lock_accp);
        for (auto it = m_acceptors.begin(); it != m_acceptors.end(); it++) {
            (*it)->stop();
        }

        return ERR_SUCCESS;
    }

    ERRCODE connection_manager::stop_all_connect()
    {
        read_lock_guard<rw_lock> lock(m_lock_conn);
        for (auto it = m_connectors.begin(); it != m_connectors.end(); it++) {
            (*it)->stop();
        }

        return ERR_SUCCESS;
    }

    ERRCODE connection_manager::stop_all_channel()
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

        return ERR_SUCCESS;
    }

    ERRCODE connection_manager::stop_all_recycle_channel()
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

        return ERR_SUCCESS;
    }

    ERRCODE connection_manager::start_listen(tcp::endpoint ep, handler_create_functor func)
    {
        try
        {
            std::weak_ptr<io_service> ios(m_acceptor_group->get_io_service());
            std::shared_ptr<tcp_acceptor> acceptor = std::make_shared<tcp_acceptor>(ios, m_worker_group, ep, func);
            ERRCODE ret = acceptor->start();
            if (ERR_SUCCESS != ret) {
                LOG_ERROR << "start listen failed at port: " << ep.port();
                return ret;
            }

            write_lock_guard<rw_lock> lock(m_lock_accp);
            m_acceptors.push_back(acceptor);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "start listen exception: " << e.what();
            return ERR_ERROR;
        }
        catch (const boost::exception & e)
        {
            LOG_ERROR << "start listen exception: " << diagnostic_information(e);
            return ERR_ERROR;
        }
        catch (...)
        {
            LOG_ERROR << "start listen exception!";
            return ERR_ERROR;
        }
        
        return ERR_SUCCESS;
    }

    ERRCODE connection_manager::stop_listen(tcp::endpoint ep)
    {
        write_lock_guard<rw_lock> lock(m_lock_accp);
        for (auto it = m_acceptors.begin(); it != m_acceptors.end(); it++)
        {
            if (ep == (*it)->get_endpoint()) {
                (*it)->stop();
                m_acceptors.erase(it);
                break;
            }
        }

        return ERR_SUCCESS;
    }

    ERRCODE connection_manager::start_connect(tcp::endpoint connect_addr, handler_create_functor func)
    {
        try
        {
            std::shared_ptr<tcp_connector> connector = std::make_shared<tcp_connector>(
                m_connector_group, m_worker_group, connect_addr, func);
            
            int32_t ret = connector->start();
            if (ERR_SUCCESS != ret) {
                LOG_ERROR << "start connect failed at addr: " << connect_addr;
                return ret;
            }

            write_lock_guard<rw_lock> lock(m_lock_conn);
            m_connectors.push_back(connector);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "start connect exception: " << e.what() << ", conn_addr=" << connect_addr;
            return E_DEFAULT;
        }
        catch (const boost::exception & e)
        {
            LOG_ERROR << "start connect exception: " << diagnostic_information(e) << ", conn_addr=" << connect_addr;
            return E_DEFAULT;
        }
        catch (...)
        {
            LOG_ERROR << "start connect exception" << ", conn_addr=" << connect_addr;
            return E_DEFAULT;
        }

        return ERR_SUCCESS;
    }

    ERRCODE connection_manager::stop_connect(tcp::endpoint connect_addr)
    {
        write_lock_guard<rw_lock> lock(m_lock_conn);
        for (auto it = m_connectors.begin(); it != m_connectors.end(); it++)
        {
            if (connect_addr == (*it)->get_connect_addr())
            {
                (*it)->stop();
                m_connectors.erase(it);
                break;
            }
        }

        return ERR_SUCCESS;
    }

    ERRCODE connection_manager::release_connector(socket_id sid)
    {
        write_lock_guard<rw_lock> lock(m_lock_conn);
        for (auto it = m_connectors.begin(); it != m_connectors.end(); it++)
        {
            if (sid != (*it)->get_socket_id()) {
                continue;
            }

            (*it)->stop();
            m_connectors.erase(it);
            return ERR_SUCCESS;
        }

        return E_NOT_FOUND;
    }

    ERRCODE connection_manager::add_channel(socket_id sid, std::shared_ptr<channel> channel)
    {
        write_lock_guard<rw_lock> lock(m_lock_chnl);

        auto ch = std::dynamic_pointer_cast<tcp_socket_channel>(channel);
        if (!check_over_max_connect(sid.get_type()))
        {
            LOG_ERROR << "add channel failed. over max connect, remote_addr:" << ch->get_remote_addr();
            return ERR_ERROR;
        }

        auto ret = m_channels.insert(make_pair(sid, channel));
        if (!ret.second) {
            LOG_ERROR << "add channel error, channel already exist"
                << ", socket_type: " << sid.get_type()
                << ", remote_addr: " << ch->get_remote_addr();
            return ERR_ERROR;
        }
        else
        {
            return ERR_SUCCESS;
        }
    }

    void connection_manager::remove_channel(socket_id sid)
    {
        write_lock_guard<rw_lock> lock(m_lock_chnl);
        m_channels.erase(sid);
    }

	std::shared_ptr<channel> connection_manager::get_channel(socket_id sid)
	{
		read_lock_guard<rw_lock> lock(m_lock_chnl);
		auto it = m_channels.find(sid);
		if (it != m_channels.end()) {
			return it->second;
		}

		return nullptr;
	}

    ERRCODE connection_manager::send_message(socket_id sid, std::shared_ptr<message> msg)
    {
		read_lock_guard<rw_lock> lock(m_lock_chnl);
        auto it = m_channels.find(sid);
        if (it == m_channels.end()) {
            return E_DEFAULT;
        }

        return it->second->write(msg);
    }

	ERRCODE connection_manager::broadcast_message(std::shared_ptr<message> msg, socket_id exclude_sid)
	{
        if (msg->content->header.nonce.empty())
        {
            LOG_ERROR << "nonce is empty, not broadcast, msg_name:" << msg->get_name();
            return ERR_ERROR;
        }

        if (!have_active_channel())
        {
            LOG_WARNING << "broadcast message error, no active channel";
            return ERR_ERROR;
        }

        {
            read_lock_guard<rw_lock> lock(m_lock_chnl);
            for (auto it = m_channels.begin(); it != m_channels.end(); ++it)
            {
                if (exclude_sid.get_id() != 0 && it->first == exclude_sid)
                    continue;

                //not login success or stopped, continue
                if (!it->second->is_channel_ready()) {
                    continue;
                }

                it->second->write(msg);
            }
        }

        return ERR_SUCCESS;
	}

    std::shared_ptr<channel> connection_manager::find_fast_path(std::vector<std::string>& path)
    {
        int i = 0;
        bool is_next_node_found = false;
        std::shared_ptr<channel> find_ch = nullptr;

        for (auto& node_id : path)
        {
            i++;
            for (auto it = m_channels.begin(); it != m_channels.end(); ++it)
            {
                //not login success or stopped, continue
                if (!it->second->is_channel_ready()) {
                    continue;
                }

                auto tcp_ch = std::dynamic_pointer_cast<tcp_socket_channel>(it->second);
                if (tcp_ch && tcp_ch->get_remote_node_id() == node_id) {
                    is_next_node_found = true;
                    find_ch = it->second;
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

        return find_ch;
    }

    bool connection_manager::have_active_channel()
    {
        if (m_channels.empty()) {
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

    bool connection_manager::send_resp_message(std::shared_ptr<message> msg, socket_id sid)
    {
        if (msg->content->header.nonce.empty()) {
            LOG_ERROR << "nonce is empty, not send resp, msg_name:" << msg->get_name();
            return false;
        }

        {
            read_lock_guard<rw_lock> lock(m_lock_chnl);

            auto &path = msg->content->header.path;
            auto ch = find_fast_path(path);
            if (ch != nullptr) {
                ch->write(msg);
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
        LOG_DEBUG << "received tcp channel error:" << sid.to_string();

        stop_channel(sid);
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
        write_lock_guard<rw_lock> lock(m_lock_recycle);

        std::queue<socket_id> goodbye_channels;
        for (auto it = m_recycle_channels.begin(); it != m_recycle_channels.end(); it++)
        {
            auto ch = std::dynamic_pointer_cast<tcp_socket_channel>(it->second);
            if (nullptr == ch)
            {
                goodbye_channels.push(it->first);
                continue;
            }

            if (!ch->is_stopped())
            {
                ch->stop();
            }
            else
            {
                //delay release 
                if (ch.use_count() <= MIN_RELEASE_CHANNEL_USE_COUNT)
                {
                    goodbye_channels.push(it->first);
                }
            }
        }

        if (!goodbye_channels.empty()) {
            LOG_INFO << "delay realse recycle channels count: " << goodbye_channels.size();
        }

        while (!goodbye_channels.empty())
        {
            socket_id sid = goodbye_channels.front();
            goodbye_channels.pop();
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
                LOG_ERROR << "on tcp channel error but not found" << sid.to_string();
                return E_DEFAULT;
            }

            ch = std::dynamic_pointer_cast<tcp_socket_channel>(it->second);
            if (nullptr == ch)
            {
                return E_DEFAULT;
            }

            //remove channel
            m_channels.erase(sid);
            LOG_DEBUG << "on tcp channel error, erase tcp socket channel" << sid.to_string();

            //stop it directly
            if (!ch->is_stopped())
            {
                LOG_DEBUG << "stop tcp socket channel" << ch->id().to_string();
                ch->stop();
            }
        }

        {
            //add recycle channel
            write_lock_guard<rw_lock> lock(m_lock_recycle);

            auto it = m_recycle_channels.find(sid);
            if (it != m_recycle_channels.end())         //already in recycle channels
            {
                LOG_ERROR << "add channel to recycle channels but found exists" << sid.to_string();
                return ERR_SUCCESS;
            }

            m_recycle_channels[sid] = ch;
            LOG_DEBUG << "on tcp channel error, add recycle tcp socket channel" << sid.to_string();
        }

        return ERR_SUCCESS;
    }

    void connection_manager::set_proto_capacity(socket_id sid, std::string c)
    {
        write_lock_guard<rw_lock> lock(m_lock_chnl);

        //get tcp socket channel
        auto it = m_channels.find(sid);
        if (it == m_channels.end())
        {
            LOG_ERROR << "on tcp channel error but not found" << sid.to_string();
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
