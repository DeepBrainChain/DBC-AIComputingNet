#ifndef DBCPROJ_VXLANMANAGER_H
#define DBCPROJ_VXLANMANAGER_H

#include "util/utils.h"
#include <leveldb/db.h>
#include <boost/process.hpp>
#include "db/db_types/network_types.h"

class VxlanManager : public Singleton<VxlanManager> {
public:
    VxlanManager();

    ERRCODE Init();

    void Exit();

    FResult CreateVxlanDevice(const std::string &bridgeName, const std::string &vxlanName, const std::string &vxlanVni);

    void DeleteVxlanDevice(const std::string &bridgeName, const std::string &vxlanName);

    FResult StartDhcpServer(const std::string &bridgeName, const std::string &vxlanName, const std::string &dhcpName,
        const std::string &netMask, const std::string &ipStart, const std::string &ipEnd, const std::string &ipLeaseNum);

    void StopDhcpServer(const std::string &bridgeName, const std::string &vxlanName);

    FResult CreateNetworkServer(const std::string &networkName, const std::string &ipCidr, const std::string &wallet);

    FResult CreateNetworkClient(const std::string &networkName);

    FResult AddNetworkFromMulticast(std::shared_ptr<dbc::networkInfo> info);

    FResult DeleteNetwork(const std::string &networkName, const std::string &wallet);

    void DeleteNetworkFromMulticast(const std::string &networkName, const std::string &machineId);

    const std::map<std::string, std::shared_ptr<dbc::networkInfo> > GetNetworks() const;

    std::shared_ptr<dbc::networkInfo> GetNetwork(const std::string &networkName) const;

    // 转移网络
    FResult MoveNetwork(const std::string &networkName, const std::string &newMachineId);

    void MoveNetworkAck(const std::string &networkName, const std::string &newMachineId);

    // 虚拟机加入网络
    void JoinNetwork(const std::string &networkName, const std::string &taskId);

    // 虚拟机离开网络
    void LeaveNetwork(const std::string &networkName, const std::string &taskId);

    // 清理长时间没有虚拟机使用的网络
    void ClearEmptyNetwork();

    // 清理长时间没有更新的网络
    void ClearExpiredNetwork();

protected:
    int32_t InitDb();

    int32_t LoadNetworkInfoFromDb();

    bool UpdateNetworkDb(std::shared_ptr<dbc::networkInfo> info);

    bool DeleteNetworkDb(const std::string &networkName);

    bool CheckVni(const std::string &vni) const;

    std::string RandomVni() const;

    bool CheckIpCidr(const std::string &ipCidr) const;

private:
    mutable RwMutex m_mtx;
    std::unique_ptr<leveldb::DB> m_db;
    std::map<std::string, std::shared_ptr<dbc::networkInfo> > m_networks;
};

#endif
