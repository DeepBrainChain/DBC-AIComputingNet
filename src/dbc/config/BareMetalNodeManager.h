#ifndef BARE_METAL_NODE_MANAGER_H
#define BARE_METAL_NODE_MANAGER_H

#include "util/utils.h"
#include "db/bare_metal_db.h"

struct bare_metal_info {
    std::string uuid;
    std::string desc;
    std::string ipmi_hostname;
    std::string ipmi_username;
    std::string ipmi_password;

    bool validate() const {
        if (uuid.empty()) return false;
        if (ipmi_hostname.empty()) return false;
        if (ipmi_username.empty()) return false;
        if (ipmi_password.empty()) return false;
        return true;
    }
};

class BareMetalNodeManager : public Singleton<BareMetalNodeManager>
{
public:
    BareMetalNodeManager();
    virtual ~BareMetalNodeManager();

    const std::map<std::string, std::shared_ptr<dbc::db_bare_metal>> getBareMetalNodes() const { return m_bare_metal_nodes; }

    ERRCODE Init();

    std::shared_ptr<dbc::db_bare_metal> getBareMetalNode(const std::string& node_id);

    bool ExistNodeID(const std::string& node_id) const;

    bool ExistUUID(const std::string& uuid) const;

    FResult AddBareMetalNodes(std::map<std::string, bare_metal_info> nodes, std::map<std::string, std::string>& ids);

    FResult DeleteBareMetalNode(const std::vector<std::string>& ids);

private:
    std::map<std::string, std::shared_ptr<dbc::db_bare_metal>> m_bare_metal_nodes;

    BareMetalDB m_db;
};


#endif //BARE_METAL_NODE_MANAGER_H
