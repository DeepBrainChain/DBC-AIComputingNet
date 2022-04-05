#ifndef DBC_SERVICE_INFO_MANAGER_H
#define DBC_SERVICE_INFO_MANAGER_H

#include "util/utils.h"
#include "message/matrix_types.h"
#include "util/filter/simple_expression.h"

class ServiceInfoManager : public Singleton<ServiceInfoManager>
{
public:
    ServiceInfoManager() = default;

    virtual ~ServiceInfoManager() = default;

    void add(const std::string& node_id, const std::shared_ptr<dbc::node_service_info>& service_info);

    void add(const std::map <std::string, std::shared_ptr<dbc::node_service_info>>& service_infos);

    int32_t size();

    const std::map<std::string, std::shared_ptr<dbc::node_service_info>>& get_all();

    std::shared_ptr<dbc::node_service_info> find(const std::string& node_id);

    void update(const std::string& node_id, const std::string& k, const std::string& v);

    void update_time_stamp(const std::string& node_id);

    void remove_unlived_nodes(int32_t sec);

    void reset_change_set();

    std::map<std::string, std::shared_ptr<dbc::node_service_info>>& get_change_set();

private:
    std::map<std::string, std::shared_ptr<dbc::node_service_info>> m_service_infos; // <node_id, service_info>
    std::map<std::string, std::shared_ptr<dbc::node_service_info>> m_change;
    std::list<std::string> m_nodeid_list;
};

#endif
