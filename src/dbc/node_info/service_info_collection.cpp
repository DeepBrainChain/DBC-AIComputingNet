#include "service_info_collection.h"
#include <cstdlib>

namespace dbc {
    void service_info_collection::reset_change_set() {
        m_change.clear();
    }

    void service_info_collection::add(const std::string& node_id, const node_service_info& service_info) {
        if (0 == m_id_2_info.count(node_id)) {
            m_id_2_info[node_id] = service_info;
            m_id_list.push_back(node_id);
        }
        else {
            auto time_a = service_info.time_stamp;
            auto time_b = m_id_2_info[node_id].time_stamp;
            if ( time_a > time_b) {
                m_id_2_info[node_id] = service_info;
            }
        }
    }

    std::shared_ptr<service_info_map> service_info_collection::get_all() {
        auto result = std::make_shared<service_info_map>();
        for (auto &it : m_id_2_info) {
            (*result)[it.first] = it.second;
        }

        return result;
    }

    std::string service_info_collection::to_string(const std::vector<std::string>& in) {
        std::string out;
        for(auto& item: in) {
            if(out.length()) {
                out += " , ";
            }

            out += item ;
        }

        return out;
    }

    service_info_map& service_info_collection::get_change_set() {
        m_change.clear();
        int max_change_set_size = m_id_list.size() < MAX_CHANGE_SET_SIZE ? m_id_list.size() : MAX_CHANGE_SET_SIZE;
        for(int i = 0; i < max_change_set_size; i++) {
            auto node_id = m_id_list.front();
            m_id_list.pop_front();
            m_id_list.push_back(node_id);

            m_change[node_id] = m_id_2_info[node_id];
        }

        return m_change;
    }

    void service_info_collection::add(const service_info_map& mp) {
        for (auto & item: mp) {
            add(item.first, item.second);
        }
    }

    void service_info_collection::update_own_node_time_stamp(const std::string& node_id) {
        if ( m_id_2_info.count(node_id)) {
            auto t = std::time(nullptr);
            m_id_2_info[node_id].__set_time_stamp(t);
        }
    }

    void service_info_collection::update(const std::string& node_id, const std::string& k, const std::string& v) {
        if ( m_id_2_info.count(node_id)) {
            m_id_2_info[node_id].kvs[k] = v;
        }
    }

    void service_info_collection::remove_unlived_nodes(int32_t time_in_second) {
        auto now = std::time(nullptr);
        time_t time_threshold = now - time_in_second;

        for (auto it = m_id_2_info.begin(); it != m_id_2_info.end();) {
            if (it->second.time_stamp < time_threshold) {
                m_id_list.remove(it->first);
                it = m_id_2_info.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    int32_t service_info_collection::size() {
        return m_id_2_info.size();
    }
}
