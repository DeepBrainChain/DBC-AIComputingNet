#include "ipmitool_client.h"
#include "config/BareMetalNodeManager.h"
#include "server/server.h"
#include "task/HttpDBCChainClient.h"

IpmiToolClient::IpmiToolClient() {
    //
}

IpmiToolClient::~IpmiToolClient() {
    //
}

FResult IpmiToolClient::Init() {
    if (Server::NodeType != NODE_TYPE::BareMetalNode)
        return FResultOk;

    // $ ipmitool -V
    // ipmitool version 1.8.18
    std::string ipmitool_ver = run_shell("ipmitool -V");
    if (ipmitool_ver.find("ipmitool version") == std::string::npos) {
        return FResult(ERR_ERROR, "ipmitool is not installed");
    }
    return FResultOk;
}

void IpmiToolClient::Exit() {
    stopped = true;
    LOG_INFO << "IpmiToolClient exited";
}

FResult IpmiToolClient::PowerControl(const std::string& node_id, const std::string& command) {
    std::shared_ptr<dbc::db_bare_metal> bm =
        BareMetalNodeManager::instance().getBareMetalNode(node_id);
    if (!bm) return FResult(ERR_ERROR, "node id not existed");

    std::string cmd = "ipmitool -H " + bm->ipmi_hostname +
        " -U " + bm->ipmi_username +
        " -P " + bm->ipmi_password +
        " chassis power " + command;
    std::string cmd_ret = run_shell(cmd);
    LOG_INFO << "run shell (" << cmd << ") return :" << cmd_ret;
    util::replace(cmd_ret, "\n", " <line> ");
    FResult fret = { ERR_SUCCESS, cmd_ret };
    if (cmd_ret.find("Error:") != std::string::npos)
        fret.errcode = ERR_ERROR;
    if (cmd_ret.find("error:") != std::string::npos)
        fret.errcode = ERR_ERROR;
    return fret;
}

FResult IpmiToolClient::SetBootDeviceOrder(const std::string& node_id, const std::string& device) {
    std::shared_ptr<dbc::db_bare_metal> bm =
        BareMetalNodeManager::instance().getBareMetalNode(node_id);
    if (!bm) return FResult(ERR_ERROR, "node id not existed");

    std::string cmd = "ipmitool -H " + bm->ipmi_hostname +
        " -U " + bm->ipmi_username +
        " -P " + bm->ipmi_password +
        " chassis bootdev " + device;
    std::string cmd_ret = run_shell(cmd);
    LOG_INFO << "run shell (" << cmd << ") return :" << cmd_ret;
    util::replace(cmd_ret, "\n", " <line> ");
    FResult fret = { ERR_SUCCESS, cmd_ret };
    if (cmd_ret.find("Error:") != std::string::npos)
        fret.errcode = ERR_ERROR;
    if (cmd_ret.find("error:") != std::string::npos)
        fret.errcode = ERR_ERROR;
    return fret;
}

void IpmiToolClient::PruneNode(const std::string& node_id) {
    MACHINE_STATUS machine_status = HttpDBCChainClient::instance().request_machine_status(node_id);
    if (machine_status == MACHINE_STATUS::Unknown) return;

    std::string cur_renter;
    int64_t cur_rent_end = 0;
    HttpDBCChainClient::instance().request_cur_renter(node_id, cur_renter, cur_rent_end);
    int64_t cur_block = HttpDBCChainClient::instance().request_cur_block();
    LOG_INFO << "prune node status is " << (int32_t)machine_status << ", current renter is " << cur_renter
        << ", rent end is " << cur_rent_end << ", cur block is " << cur_block;
    if (machine_status == MACHINE_STATUS::Online) {
        PowerControl(node_id, "off");
    } else if (machine_status == MACHINE_STATUS::Rented) {
        if (cur_rent_end > 0 && cur_block > cur_rent_end) {
            PowerControl(node_id, "off");
        }
    }
}

void IpmiToolClient::PruneNodes() {
    HttpDBCChainClient::instance().update_chain_order(ConfManager::instance().GetDbcChainDomain());

    std::vector<std::string> ids;
    {
        const std::map<std::string, std::shared_ptr<dbc::db_bare_metal>> bm_nodes = 
            BareMetalNodeManager::instance().getBareMetalNodes();
        for (const auto& iter : bm_nodes)
            ids.push_back(iter.first);
    }

    for (const auto& id : ids) {
        if (stopped) break;
        PruneNode(id);
        sleep(3);
    }
}
