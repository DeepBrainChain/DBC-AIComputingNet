#ifndef DBC_NETWORK_CONNECTION_MANAGER_H
#define DBC_NETWORK_CONNECTION_MANAGER_H

#include "nio_loop_group.h"
#include "tcp_acceptor.h"
#include "tcp_connector.h"
#include "rw_lock.h"
#include "channel.h"
#include "compress/matrix_capacity.h"
#include "service_module/service_module.h"

using namespace std;

#define DEFAULT_ACCEPTOR_THREAD_COUNT             1
#define DEFAULT_CONNECTOR_THREAD_COUNT            1
#define DEFAULT_WORKER_THREAD_COUNT               8
#define MIN_RELEASE_CHANNEL_USE_COUNT             2

#ifdef WIN32
// Win32 LevelDB doesn't use filedescriptors, and the ones used for
// accessing block files don't count towards the fd_set size limit
// anyway.
#define MIN_CORE_FILEDESCRIPTORS 0
#else
#define MIN_CORE_FILEDESCRIPTORS 150
#endif

/** Maximum number of automatic outgoing nodes */
static const int MAX_OUTBOUND_CONNECTIONS = 168;
/** Maximum number of addnode outgoing nodes */
static const int MAX_ADDNODE_CONNECTIONS = 168;

#ifdef FD_SETSIZE
#undef FD_SETSIZE // prevent redefinition compiler warning
#endif
#define FD_SETSIZE 1024 // max number of fds in fd_set

namespace dbc
{
    namespace network
    {
        class connection_manager : public matrix::core::service_module
        {
        public:
            using nio_loop_ptr = typename std::shared_ptr<nio_loop_group>;

            connection_manager();

            ~connection_manager() override = default;

            std::string module_name() const override { return connection_manager_name; }

            void set_proto_capacity(socket_id sid, std::string c);

            int32_t start_listen(tcp::endpoint ep, handler_create_functor func);

            int32_t stop_listen(tcp::endpoint ep);

            int32_t start_connect(tcp::endpoint connect_addr, handler_create_functor func);

            int32_t stop_connect(tcp::endpoint connect_addr);

            int32_t release_connector(socket_id sid);

            int32_t add_channel(socket_id sid, shared_ptr<channel> channel);

            void remove_channel(socket_id sid);

            int32_t stop_channel(socket_id sid);

			shared_ptr<channel> get_channel(socket_id sid);

            size_t get_connection_num() { return m_channels.size();}

            bool have_active_channel();

            shared_ptr<channel> find_fast_path(std::vector<std::string>& path);

            int32_t send_message(socket_id sid, std::shared_ptr<message> msg);

			int32_t broadcast_message(std::shared_ptr<message> msg, socket_id id = socket_id());

            bool send_resp_message(std::shared_ptr<message> msg, socket_id id = socket_id());

        protected:
            int32_t service_init(bpo::variables_map &options) override;

            int32_t service_exit() override;

            void init_invoker() override;

            void init_timer() override;

            void init_subscription() override;

            virtual int32_t init_io_services();

            virtual int32_t exit_io_services();

            //io_services
            virtual int32_t start_io_services();

            virtual int32_t stop_io_services();

            //all listen and connect
            virtual int32_t stop_all_listen();

            virtual int32_t stop_all_connect();

            virtual int32_t stop_all_channel();

            int32_t get_connect_num();
            int32_t get_out_connect_num();
            int32_t get_in_connect_num();

            bool check_over_max_connect(socket_type st);

            virtual int32_t stop_all_recycle_channel();

            int32_t on_tcp_channel_error(std::shared_ptr<message> &msg);

            int32_t on_recycle_timer(std::shared_ptr<matrix::core::core_timer> timer);

            virtual void remove_timers();

            int32_t load_max_connect(bpo::variables_map &options);

        protected:
            uint32_t m_channel_recycle_timer = INVALID_TIMER_ID;

            rw_lock m_lock_conn;//connector
			rw_lock m_lock_accp;//acceptor
			rw_lock m_lock_chnl;//channels
            rw_lock m_lock_recycle;     //recycle channels

            //io service group
            nio_loop_ptr m_worker_group;
            nio_loop_ptr m_acceptor_group;
            nio_loop_ptr m_connector_group;

            //acceptor
            list<shared_ptr<tcp_acceptor>> m_acceptors;
            //connector
            list<shared_ptr<tcp_connector>> m_connectors;

            //socket channel
            std::map<socket_id, shared_ptr<channel>, cmp_key> m_channels;
            int32_t m_max_connect = 512;
            //recycle channel for exception tcp socket channel
            std::map<socket_id, shared_ptr<channel>, cmp_key> m_recycle_channels;
        };
    }
}

#endif
