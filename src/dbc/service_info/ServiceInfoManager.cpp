#include "ServiceInfoManager.h"
#include <cstdlib>

#define MAX_CHANGE_SET_SIZE 48

void ServiceInfoManager::add(const std::string& node_id, const std::shared_ptr<dbc::node_service_info>& service_info) {
    if (0 == m_service_infos.count(node_id)) {
        m_service_infos[node_id] = service_info;
        m_nodeid_list.push_back(node_id);
    }
    else {
        auto time_a = service_info->time_stamp;
        auto time_b = m_service_infos[node_id]->time_stamp;
        if ( time_a > time_b) {
            m_service_infos[node_id] = service_info;
        }
    }
}

void ServiceInfoManager::add(const std::map<std::string, std::shared_ptr<dbc::node_service_info>>& service_infos) {
    for (auto& item : service_infos) {
        add(item.first, item.second);
    }
}

int32_t ServiceInfoManager::size() {
    return m_service_infos.size();
}

const std::map <std::string, std::shared_ptr<dbc::node_service_info>>& ServiceInfoManager::get_all() {
    return m_service_infos;
}

std::shared_ptr<dbc::node_service_info> ServiceInfoManager::find(const std::string& node_id) {
    auto it = m_service_infos.find(node_id);
    if (it == m_service_infos.end()) {
        return nullptr;
    }
    else {
        return it->second;
    }
}

void ServiceInfoManager::update(const std::string& node_id, const std::string& k, const std::string& v) {
    auto it = m_service_infos.find(node_id);
    if (it != m_service_infos.end()) {
        it->second->kvs[k] = v;
    }
}

void ServiceInfoManager::update_time_stamp(const std::string& node_id) {
    auto it = m_service_infos.find(node_id);
    if (it != m_service_infos.end()) {
        auto t = std::time(nullptr);
        it->second->__set_time_stamp(t);
    }
}

void ServiceInfoManager::remove_unlived_nodes(int32_t sec) {
    auto now = std::time(nullptr);
    time_t time_threshold = now - sec;

    for (auto it = m_service_infos.begin(); it != m_service_infos.end();) {
        if (it->second->time_stamp < time_threshold) {
            m_nodeid_list.remove(it->first);
            it = m_service_infos.erase(it);
        }
        else {
            ++it;
        }
    }
}

void ServiceInfoManager::reset_change_set() {
    m_change.clear();
}

std::map <std::string, std::shared_ptr<dbc::node_service_info>>& ServiceInfoManager::get_change_set() {
    m_change.clear();

    int max_change_set_size = m_nodeid_list.size() < MAX_CHANGE_SET_SIZE ? m_nodeid_list.size() : MAX_CHANGE_SET_SIZE;
    for(int i = 0; i < max_change_set_size; i++) {
        auto node_id = m_nodeid_list.front();
        m_nodeid_list.pop_front();
        m_nodeid_list.push_back(node_id);

        m_change[node_id] = m_service_infos[node_id];
    }

    return m_change;
}
