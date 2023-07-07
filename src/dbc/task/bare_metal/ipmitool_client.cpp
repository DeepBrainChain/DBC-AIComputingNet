#include "ipmitool_client.h"

#include "log/log.h"
#include "task/bare_metal/bare_metal_node_manager.h"

IpmiToolClient::IpmiToolClient() {
    //
}

IpmiToolClient::~IpmiToolClient() {
    //
}

FResult IpmiToolClient::Init() {
    // $ ipmitool -V
    // ipmitool version 1.8.18
    std::string ipmitool_ver = run_shell("ipmitool -V");
    if (ipmitool_ver.find("ipmitool version") == std::string::npos) {
        return FResult(ERR_ERROR, "ipmitool is not installed");
    }
    return FResultOk;
}

void IpmiToolClient::Exit() {
    // stopped = true;
    LOG_INFO << "IpmiToolClient exited";
}

FResult IpmiToolClient::PowerControl(const std::string& node_id,
                                     const std::string& command) {
    std::shared_ptr<dbc::db_bare_metal> bm =
        BareMetalNodeManager::instance().getBareMetalNode(node_id);
    if (!bm) return FResult(ERR_ERROR, "node id not existed");

    std::string cmd = "ipmitool -H " + bm->ipmi_hostname;
    if (!bm->ipmi_port.empty()) cmd += " -p " + bm->ipmi_port;
    cmd += " -U " + bm->ipmi_username + " -P " + bm->ipmi_password +
           " chassis power " + command;
    std::string cmd_ret = run_shell(cmd);
    LOG_INFO << "run shell (" << cmd << ") return :" << cmd_ret;
    util::replace(cmd_ret, "\n", " <line> ");
    FResult fret = {ERR_SUCCESS, cmd_ret};
    if (cmd_ret.find("Error:") != std::string::npos) fret.errcode = ERR_ERROR;
    if (cmd_ret.find("error:") != std::string::npos) fret.errcode = ERR_ERROR;
    return fret;
}

FResult IpmiToolClient::SetBootDeviceOrder(const std::string& node_id,
                                           const std::string& device) {
    std::shared_ptr<dbc::db_bare_metal> bm =
        BareMetalNodeManager::instance().getBareMetalNode(node_id);
    if (!bm) return FResult(ERR_ERROR, "node id not existed");

    std::string cmd = "ipmitool -H " + bm->ipmi_hostname;
    if (!bm->ipmi_port.empty()) cmd += " -p " + bm->ipmi_port;
    cmd += " -U " + bm->ipmi_username + " -P " + bm->ipmi_password +
           " chassis bootdev " + device;
    std::string cmd_ret = run_shell(cmd);
    LOG_INFO << "run shell (" << cmd << ") return :" << cmd_ret;
    util::replace(cmd_ret, "\n", " <line> ");
    FResult fret = {ERR_SUCCESS, cmd_ret};
    if (cmd_ret.find("Error:") != std::string::npos) fret.errcode = ERR_ERROR;
    if (cmd_ret.find("error:") != std::string::npos) fret.errcode = ERR_ERROR;
    return fret;
}
