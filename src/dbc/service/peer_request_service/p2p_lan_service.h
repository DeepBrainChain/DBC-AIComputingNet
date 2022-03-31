#ifndef DBC_P2P_LAN_SERVICE_H
#define DBC_P2P_LAN_SERVICE_H

#include "util/utils.h"
#include "common/common.h"
#include <leveldb/db.h>
#include "service_module/service_module.h"
#include "multicast/multicast_reveiver.h"
#include "multicast/multicast_sender.h"
#include "network/utils/io_service_pool.h"

#define AI_MULTICAST_MACHINE_TIMER                                   "multicast_machine_task"
#define AI_MULTICAST_NETWORK_TIMER                                   "multicast_network_task"

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

    void on_multicast_receive(const std::string& data, const std::string& addr);

protected:
    void init_timer() override;

    void init_invoker() override;

    void init_local_machine_info();

    void on_multicast_machine_task_timer(const std::shared_ptr<core_timer>& timer);

    void on_multicast_network_task_timer(const std::shared_ptr<core_timer>& timer);

    void send_machine_exit_request() const;

private:
    std::shared_ptr<network::io_service_pool> m_io_service_pool = nullptr;
    std::shared_ptr<multicast_receiver> m_receiver = nullptr;
    std::shared_ptr<multicast_sender> m_sender = nullptr;
    lan_machine_info m_local_machine_info;
    std::string m_local_machine_message;

    mutable RwMutex m_mtx;
    std::map<std::string, lan_machine_info> m_lan_nodes;
};

#endif
