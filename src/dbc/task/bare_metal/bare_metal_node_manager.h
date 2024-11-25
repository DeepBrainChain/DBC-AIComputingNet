#ifndef BARE_METAL_NODE_MANAGER_H
#define BARE_METAL_NODE_MANAGER_H

#include "db/bare_metal_db.h"
#include "util/utils.h"

struct bare_metal_info {
    std::string uuid;
    std::string ip;
    std::string os;
    std::string desc;
    std::string ipmi_hostname;
    std::string ipmi_username;
    std::string ipmi_password;
    uint32_t ipmi_port = 0;

    bool validate() const {
        if (uuid.empty()) return false;
        for (const auto& ch : uuid) {
            if (!isalnum(ch) && ch != '-') return false;
        }
        if (ip.empty()) return false;
        if (ipmi_hostname.empty()) return false;
        if (ipmi_username.empty()) return false;
        if (ipmi_password.empty()) return false;
        if (ipmi_port > 65535) return false;
        return true;
    }
};

class BareMetalNodeManager : public Singleton<BareMetalNodeManager> {
public:
    BareMetalNodeManager();
    virtual ~BareMetalNodeManager();

    ERRCODE Init();

    const std::map<std::string, std::shared_ptr<dbc::db_bare_metal>>
    getBareMetalNodes() const;

    std::shared_ptr<dbc::db_bare_metal> getBareMetalNode(
        const std::string& node_id);

    bool ExistNodeID(const std::string& node_id) const;

    bool ExistUUID(const std::string& uuid) const;

    FResult AddBareMetalNodes(std::map<std::string, bare_metal_info> nodes,
                              std::map<std::string, std::string>& ids);

    FResult DeleteBareMetalNode(const std::vector<std::string>& ids);

    FResult ModifyBareMetalNode(const std::string& node_id,
                                const bare_metal_info& bmi);

    [[deprecated]] FResult SetDeepLinkInfo(const std::string& node_id,
                                           const std::string& device_id,
                                           const std::string& device_password);

    [[deprecated]] void ClearDeepLinkPassword(const std::string& node_id);

private:
    mutable RwMutex m_mtx;

    std::map<std::string, std::shared_ptr<dbc::db_bare_metal>>
        m_bare_metal_nodes;

    BareMetalDB m_db;
};

#endif  // BARE_METAL_NODE_MANAGER_H
