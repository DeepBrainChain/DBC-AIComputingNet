#ifndef DBC_NETWORK_CONNECTION_MANAGER_H
#define DBC_NETWORK_CONNECTION_MANAGER_H

#include "utils/io_service_pool.h"
#include "tcp_acceptor.h"
#include "tcp_connector.h"
#include "util/rw_lock.h"
#include "channel/channel.h"
#include "compress/matrix_capacity.h"
#include "service_module/service_module.h"

#define DEFAULT_ACCEPTOR_THREAD_COUNT             1
#define DEFAULT_CONNECTOR_THREAD_COUNT            1
#define DEFAULT_WORKER_THREAD_COUNT               8

#define MIN_RELEASE_CHANNEL_USE_COUNT             2
#define MIN_CORE_FILEDESCRIPTORS        150

/** Maximum number of automatic outgoing nodes */
static const int MAX_OUTBOUND_CONNECTIONS = 168;
/** Maximum number of addnode outgoing nodes */
static const int MAX_ADDNODE_CONNECTIONS = 168;

#ifdef FD_SETSIZE
#undef FD_SETSIZE 
#endif
#define FD_SETSIZE 1024

namespace network
{
    class connection_manager : public service_module, public Singleton<connection_manager>
    {
    public:
        connection_manager() = default;

        ~connection_manager() override = default;

        void set_proto_capacity(socket_id sid, std::string c);

        ERRCODE init() override;

        void exit() override;

        ERRCODE start_listen(tcp::endpoint ep, handler_create_functor func);

        ERRCODE stop_listen(tcp::endpoint ep);

        ERRCODE start_connect(tcp::endpoint connect_addr, handler_create_functor func);

        ERRCODE stop_connect(tcp::endpoint connect_addr);
        
        ERRCODE release_connector(socket_id sid);

        ERRCODE add_channel(socket_id sid, std::shared_ptr<channel> channel);

        void remove_channel(socket_id sid);

        ERRCODE stop_channel(socket_id sid);
        
		std::shared_ptr<channel> get_channel(socket_id sid);

        size_t get_connection_num() { return m_channels.size();}

        bool have_active_channel(); //CHANNEL_ACTIVE && logined

        ERRCODE broadcast_message(std::shared_ptr<message> msg, socket_id exclude_sid = socket_id());

        ERRCODE send_message(socket_id sid, std::shared_ptr<message> msg);

        std::shared_ptr<channel> find_fast_path(std::vector<std::string>& path);

        bool send_resp_message(std::shared_ptr<message> msg, socket_id sid = socket_id());

    protected:
        void init_invoker() override;

        void init_timer() override;

        ERRCODE init_io_services();

        ERRCODE start_io_services();

        ERRCODE stop_io_services();

        ERRCODE exit_io_services();
        
        ERRCODE load_max_connect();

        ERRCODE stop_all_listen();

        ERRCODE stop_all_connect();

        ERRCODE stop_all_channel();

        int32_t get_connect_num(); // CHANNEL_ACTIVE

        int32_t get_out_connect_num(); // CHANNEL_ACTIVE

        int32_t get_in_connect_num(); // CHANNEL_ACTIVE

        bool check_over_max_connect(socket_type st);

        virtual int32_t stop_all_recycle_channel();

        void on_tcp_channel_error(const std::shared_ptr<message> &msg);

        void on_recycle_timer(const std::shared_ptr<core_timer>& timer);
        
    protected:
        rw_lock m_lock_conn; //connector
		rw_lock m_lock_accp; //acceptor
		rw_lock m_lock_chnl; //channels
        rw_lock m_lock_recycle; //recycle_channels
        
        std::shared_ptr<io_service_pool> m_worker_group;
        std::shared_ptr<io_service_pool> m_acceptor_group;
        std::shared_ptr<io_service_pool> m_connector_group;

        int32_t m_max_connect = 1024;

        std::list<std::shared_ptr<tcp_acceptor>> m_acceptors;
        std::list<std::shared_ptr<tcp_connector>> m_connectors;

        std::map<socket_id, std::shared_ptr<channel>, cmp_key> m_channels;
        std::map<socket_id, std::shared_ptr<channel>, cmp_key> m_recycle_channels;
    };
}

#endif
