#ifndef DBCPROJ_VXLANMANAGER_H
#define DBCPROJ_VXLANMANAGER_H

#include "util/utils.h"
#include <leveldb/db.h>
#include <boost/process.hpp>
#include "service/task/db/network_types.h"

class VxlanManager : public Singleton<VxlanManager> {
public:
    VxlanManager();

    ERRCODE Init();

    FResult CreateNetwork(const std::string &networkId, const std::string &bridgeName, const std::string &vxlanName, const std::string &vni, const std::string &ipCidr);

    FResult CreateClientNetwork(const std::string &networkId, const std::string &vni, const std::string &ipCidr);

    FResult CreateMiningNetwork(const std::string &networkId, const std::string &vni);

    void DeleteNetwork(const std::string &networkId);

    const std::map<std::string, std::shared_ptr<dbc::networkInfo> > GetNetworks() const;

    std::shared_ptr<dbc::networkInfo> GetNetwork(const std::string &networkId) const;

    bool CheckVni(const std::string &vni) const;

    bool CheckIpCidr(const std::string &ipCidr) const;

protected:
    int32_t InitDb();

    int32_t LoadNetworkInfoFromDb();

private:
    mutable RwMutex m_mtx;
    std::unique_ptr<leveldb::DB> m_db;
    std::map<std::string, std::shared_ptr<dbc::networkInfo> > m_networks;
};

#endif
