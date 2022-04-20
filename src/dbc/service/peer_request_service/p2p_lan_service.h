#ifndef DBC_P2P_LAN_SERVICE_H
#define DBC_P2P_LAN_SERVICE_H

#include "util/utils.h"
#include "common/common.h"
#include <leveldb/db.h>
#include "service_module/service_module.h"
#include "multicast/multicast_reveiver.h"
#include "multicast/multicast_sender.h"
#include "network/utils/io_service_pool.h"
#include "db/db_types/network_types.h"

#define AI_MULTICAST_MACHINE_TIMER                                   "multicast_machine_task"
#define AI_MULTICAST_NETWORK_TIMER                                   "multicast_network_task"
#define AI_MULTICAST_NETWORK_RESUME_TIMER                            "multicast_network_resume_task"

struct lan_machine_info {
    std::string machine_id;
    std::string net_type;
    int32_t net_flag;
    std::string local_address;
    short local_port;
    NODE_TYPE node_type;
    time_t cur_time;

    bool validate() const;
    std::string to_request_json() const;
};

class p2p_lan_service : public service_module, public Singleton<p2p_lan_service> {
public:
    p2p_lan_service() = default;

    ~p2p_lan_service() override = default;

    ERRCODE init() override;

    void exit() override;

    const std::map<std::string, lan_machine_info>& get_lan_nodes() const;

    void add_lan_node(const lan_machine_info& lm_info);

    void delete_lan_node(const std::string& machine_id);

    bool is_same_host(const std::string& machine_id) const;

    void on_multicast_receive(const std::string& data, const std::string& addr);

    void send_network_create_request(std::shared_ptr<dbc::networkInfo> info);

    void send_network_delete_request(const std::string& network_name);

    void send_network_query_request();

    void send_network_list_request();

    void send_network_move_request(const std::string& network_name, const std::string& old_machine_id);

    void send_network_move_ack_request(const std::string& network_name, const std::string& machine_id);

    void send_network_join_request(const std::string& network_name, const std::string& task_id);

    void send_network_leave_request(const std::string& network_name, const std::string& task_id);

protected:
    void init_timer() override;

    void init_invoker() override;

    void init_local_machine_info();

    void on_multicast_machine_task_timer(const std::shared_ptr<core_timer>& timer);

    void on_multicast_network_task_timer(const std::shared_ptr<core_timer>& timer);

    void on_multicast_network_move_task_timer(const std::shared_ptr<core_timer>& timer);

    void on_multicast_network_resume_task_timer(const std::shared_ptr<core_timer>& timer);

    void send_machine_exit_request() const;

private:
    std::shared_ptr<network::io_service_pool> m_io_service_pool = nullptr;
    std::shared_ptr<multicast_receiver> m_receiver = nullptr;
    std::shared_ptr<multicast_sender> m_sender = nullptr;
    lan_machine_info m_local_machine_info;
    std::string m_local_machine_message;

    mutable RwMutex m_mtx;
    std::map<std::string, lan_machine_info> m_lan_nodes;

    mutable RwMutex m_mtx_timer;
    std::map<std::string, int32_t> m_network_move_timer_id;
};

#endif
