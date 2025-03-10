#include "bare_metal_node_manager.h"

#include "server/server.h"
#include "service/node_request_service/node_request_service.h"

BareMetalNodeManager::BareMetalNodeManager() {}

BareMetalNodeManager::~BareMetalNodeManager() {}

ERRCODE BareMetalNodeManager::Init() {
    if (Server::NodeType == NODE_TYPE::BareMetalNode) {
        bool ret =
            m_db.init_db(EnvManager::instance().get_db_path(), "bare_metal.db");
        if (!ret) {
            LOG_ERROR << "init bare_metal_db failed";
            return ERR_ERROR;
        }

        m_db.load_datas(m_bare_metal_nodes);
    }
    return ERR_SUCCESS;
}

const std::map<std::string, std::shared_ptr<dbc::db_bare_metal>>
BareMetalNodeManager::getBareMetalNodes() const {
    RwMutex::ReadLock rlock(m_mtx);
    return m_bare_metal_nodes;
}

std::shared_ptr<dbc::db_bare_metal> BareMetalNodeManager::getBareMetalNode(
    const std::string& node_id) {
    RwMutex::ReadLock rlock(m_mtx);
    auto iter = m_bare_metal_nodes.find(node_id);
    if (iter != m_bare_metal_nodes.end()) return iter->second;
    return nullptr;
}

bool BareMetalNodeManager::ExistNodeID(const std::string& node_id) const {
    RwMutex::ReadLock rlock(m_mtx);
    // for (const auto& iter : m_bare_metal_nodes) {
    //     LOG_INFO << "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq " <<
    //     iter.first;
    // }
    // return m_bare_metal_nodes.count(nodeID) > 0;
    return m_bare_metal_nodes.find(node_id) != m_bare_metal_nodes.end();
}

bool BareMetalNodeManager::ExistUUID(const std::string& uuid) const {
    RwMutex::ReadLock rlock(m_mtx);
    bool bExisted = false;
    for (const auto& iter : m_bare_metal_nodes) {
        if (iter.second->uuid == uuid) {
            bExisted = true;
            break;
        }
    }
    return bExisted;
}

FResult BareMetalNodeManager::AddBareMetalNodes(
    std::map<std::string, bare_metal_info> nodes,
    std::map<std::string, std::string>& ids) {
    RwMutex::WriteLock wlock(m_mtx);

    FResult fret = {ERR_ERROR, ""};
    for (const auto& iter : nodes) {
        if (!iter.second.validate()) {
            fret.errmsg = "invalid bare metal info";
            return fret;
        }
        util::machine_node_info info;
        int32_t ret = util::create_node_info(info);
        if (0 != ret || m_bare_metal_nodes.count(info.node_id) > 0) {
            fret.errmsg = "create node id for bare metal error";
            LOG_ERROR << fret.errmsg;
            return fret;
        }
        std::shared_ptr<dbc::db_bare_metal> bm =
            std::make_shared<dbc::db_bare_metal>();
        bm->__set_node_id(info.node_id);
        bm->__set_node_private_key(info.node_private_key);
        bm->__set_uuid(iter.second.uuid);
        bm->__set_ip(iter.second.ip);
        bm->__set_os(iter.second.os);
        bm->__set_desc(iter.second.desc);
        bm->__set_ipmi_hostname(iter.second.ipmi_hostname);
        bm->__set_ipmi_username(iter.second.ipmi_username);
        bm->__set_ipmi_password(iter.second.ipmi_password);
        if (iter.second.ipmi_port > 0 && iter.second.ipmi_port <= 65535)
            bm->__set_ipmi_port(std::to_string(iter.second.ipmi_port));
        if (!m_db.write_data(bm)) {
            fret.errmsg = "write bare metal db error";
            LOG_ERROR << fret.errmsg;
            return fret;
        }
        m_bare_metal_nodes[info.node_id] = bm;
        node_request_service::instance().add_self_to_servicelist(info.node_id);
        ids[iter.second.uuid] = info.node_id;
        LOG_INFO << "add bare metal node successful, uuid: " << iter.second.uuid
                 << ", ip: " << iter.second.ip << ", os: " << iter.second.os
                 << ", desc: " << iter.second.desc
                 << ", ipmi hostname: " << iter.second.ipmi_hostname
                 << ", ipmi username: " << iter.second.ipmi_username
                 << ", ipmi password: " << iter.second.ipmi_password
                 << ", ipmi port: " << iter.second.ipmi_port;
    }

    fret.errcode = ERR_SUCCESS;
    fret.errmsg = "ok";
    return fret;
}

FResult BareMetalNodeManager::DeleteBareMetalNode(
    const std::vector<std::string>& ids) {
    RwMutex::WriteLock wlock(m_mtx);

    FResult fret = {-1, "ids is empty"};

    if (ids.empty()) return fret;

    int succ = 0;

    for (const auto& id : ids) {
        if (m_bare_metal_nodes.count(id) > 0) {
            if (m_db.delete_data(id)) {
                m_bare_metal_nodes.erase(id);
                ++succ;
                LOG_INFO << "delete bare metal node id " << id
                         << " successfuly";
            } else {
                fret.errmsg = "delete bare metal node db failed";
                LOG_ERROR << "delete bare metal node id " << id << " failed";
            }
        } else {
            fret.errmsg = "bare metal node id " + id + " not existed";
        }
    }

    if (succ > 0) {
        fret.errcode = succ;
        fret.errmsg = std::to_string(succ) + " were successfully deleted, " +
                      std::to_string(ids.size() - succ) + " failed";
    }
    return fret;
}

FResult BareMetalNodeManager::ModifyBareMetalNode(const std::string& node_id,
                                                  const bare_metal_info& bmi) {
    FResult fret = {ERR_ERROR, ""};
    {
        RwMutex::ReadLock rlock(m_mtx);
        bool bExisted = false;
        for (const auto& iter : m_bare_metal_nodes) {
            if (iter.first == node_id) {
                bExisted = true;
                continue;
            }
            if (iter.second->uuid == bmi.uuid) {
                fret.errmsg =
                    "bare metal node uuid " + bmi.uuid + " already existed";
                return fret;
            }
        }
        if (!bExisted) {
            fret.errmsg = "node id not existed";
            return fret;
        }
    }
    RwMutex::WriteLock wlock(m_mtx);
    auto bm = m_bare_metal_nodes[node_id];
    if (!bmi.uuid.empty()) bm->__set_uuid(bmi.uuid);
    if (!bmi.ip.empty()) bm->__set_ip(bmi.ip);
    if (!bmi.os.empty()) bm->__set_os(bmi.os);
    if (!bmi.desc.empty()) bm->__set_desc(bmi.desc);
    if (!bmi.ipmi_hostname.empty()) bm->__set_ipmi_hostname(bmi.ipmi_hostname);
    if (!bmi.ipmi_username.empty()) bm->__set_ipmi_username(bmi.ipmi_username);
    if (!bmi.ipmi_password.empty()) bm->__set_ipmi_password(bmi.ipmi_password);
    if (bmi.ipmi_port > 0 && bmi.ipmi_port <= 65535)
        bm->__set_ipmi_port(std::to_string(bmi.ipmi_port));
    if (!m_db.write_data(bm)) {
        fret.errmsg = "write bare metal db error";
        LOG_ERROR << fret.errmsg;
        return fret;
    }
    m_bare_metal_nodes[node_id] = bm;
    fret.errcode = ERR_SUCCESS;
    LOG_INFO << "modify bare metal node successful, uuid: " << bm->uuid
             << ", ip: " << bm->ip << ", os: " << bm->os
             << ", desc: " << bm->desc
             << ", ipmi hostname: " << bm->ipmi_hostname
             << ", ipmi username: " << bm->ipmi_username
             << ", ipmi password: " << bm->ipmi_password
             << ", ipmi port: " << bm->ipmi_port;
    return fret;
}

FResult BareMetalNodeManager::SetDeepLinkInfo(
    const std::string& node_id, const std::string& device_id,
    const std::string& device_password) {
    FResult fret = {ERR_ERROR, ""};
    {
        RwMutex::ReadLock rlock(m_mtx);
        if (m_bare_metal_nodes.find(node_id) == m_bare_metal_nodes.end()) {
            fret.errmsg = "node id not existed";
            return fret;
        }
    }
    RwMutex::WriteLock wlock(m_mtx);
    auto bm = m_bare_metal_nodes[node_id];
    bm->__set_deeplink_device_id(device_id);
    bm->__set_deeplink_device_password(device_password);
    if (!m_db.write_data(bm)) {
        fret.errmsg = "write bare metal db error";
        return fret;
    }
    m_bare_metal_nodes[node_id] = bm;
    fret.errcode = ERR_SUCCESS;
    fret.errmsg = "ok";
    return fret;
}

void BareMetalNodeManager::ClearDeepLinkPassword(const std::string& node_id) {
    {
        RwMutex::ReadLock rlock(m_mtx);
        auto iter = m_bare_metal_nodes.find(node_id);
        if (iter == m_bare_metal_nodes.end()) return;
        if (iter->second->deeplink_device_password.empty()) return;
    }
    RwMutex::WriteLock wlock(m_mtx);
    auto bm = m_bare_metal_nodes[node_id];
    bm->__set_deeplink_device_password("");
    if (!m_db.write_data(bm)) {
        LOG_ERROR
            << "write bare metal db error while clear deeplink device password"
            << " of " << node_id;
        return;
    }
    m_bare_metal_nodes[node_id] = bm;
    LOG_INFO << "clear deeplink device password succcess of " << node_id;
}
