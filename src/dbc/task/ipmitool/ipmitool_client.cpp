#include "ipmitool_client.h"
#include "config/BareMetalNodeManager.h"
#include "server/server.h"

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
    //
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
