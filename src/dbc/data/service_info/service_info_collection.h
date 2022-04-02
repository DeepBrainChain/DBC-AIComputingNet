#ifndef DBC_SERVICE_INFO_COLLECTION_H
#define DBC_SERVICE_INFO_COLLECTION_H

#include "util/utils.h"
#include "message/matrix_types.h"
#include "util/filter/simple_expression.h"
#include <boost/serialization/singleton.hpp>

using namespace boost::serialization;

typedef std::map <std::string, dbc::node_service_info> service_info_map;

enum {
    MAX_CHANGE_SET_SIZE = 48,
    MAX_STORED_SERVICE_INFO_NUM = 10000
};

class service_info_collection : public Singleton<service_info_collection>
{
public:
    service_info_collection() = default;

    virtual ~service_info_collection() = default;

    void add(const std::string& node_id, const dbc::node_service_info& service_info);

    void add(const service_info_map& mp);

    int32_t size();

    std::shared_ptr<service_info_map> get_all();
    void update(const std::string& node_id, const std::string& k, const std::string& v);

    void update_own_node_time_stamp(const std::string& node_id);
    void remove_unlived_nodes(int32_t time_in_second);

    void reset_change_set();
    service_info_map& get_change_set();

    bool find(const std::string& node_id, dbc::node_service_info& service_info);

    std::string to_string(const std::vector<std::string>& in);

private:
    service_info_map m_id_2_info; // <node_id, service_info>
    service_info_map m_change;
    std::list<std::string> m_id_list;
};

#endif
