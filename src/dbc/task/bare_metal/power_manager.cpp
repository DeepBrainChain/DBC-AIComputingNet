#include "power_manager.h"

#include <boost/process.hpp>

#include "config/conf_manager.h"
#include "log/log.h"
#include "task/bare_metal/bare_metal_node_manager.h"

static std::string GetBareMetalInfo(std::shared_ptr<dbc::db_bare_metal> bm) {
    std::stringstream ss;
    ss << "'{";
    ss << "\"node_id\":"
       << "\"" << bm->node_id << "\"";
    ss << ",\"uuid\":"
       << "\"" << bm->uuid << "\"";
    ss << ",\"ip\":"
       << "\"" << bm->ip << "\"";
    ss << ",\"os\":"
       << "\"" << bm->os << "\"";
    ss << ",\"description\":"
       << "\"" << bm->desc << "\"";
    ss << ",\"ipmi_hostname\":"
       << "\"" << bm->ipmi_hostname << "\"";
    ss << ",\"ipmi_username\":"
       << "\"" << bm->ipmi_username << "\"";
    ss << ",\"ipmi_password\":"
       << "\"" << bm->ipmi_password << "\"";
    if (!bm->ipmi_port.empty()) ss << ",\"ipmi_port\":" << bm->ipmi_port;
    ss << "}'";
    return ss.str();
}

static FResult RunPowerCommand(const std::string& file,
                               const std::string& command,
                               const std::string& machine) {
    std::string result;
    try {
        boost::process::ipstream is;  // reading pipe-stream
        std::vector<std::string> args{command, machine};
        boost::process::child c(file, args, boost::process::std_out > is,
                                boost::process::std_err > boost::process::null);

        std::string line;
        while (c.running() && std::getline(is, line)) {
            result += line;
        }

        c.wait();
        return FResult(c.exit_code(), result);
    } catch (boost::exception& e) {
        FResult(ERR_ERROR, diagnostic_information(e));
    } catch (...) {
    }

    return FResult(ERR_ERROR, "run power manager tool error");
}

namespace fool {

PowerManager::PowerManager() {
    //
}

PowerManager::~PowerManager() {
    //
}

FResult PowerManager::Init() {
    power_manager_tool_ = ConfManager::instance().GetNetbarPowerManager();
    if (!boost::filesystem::exists(power_manager_tool_))
        return FResult(ERR_ERROR, "power manager tool not existed");
    return FResultOk;
}

void PowerManager::Exit() {
    // stopped = true;
    LOG_INFO << "PowerManager exited";
}

FResult PowerManager::PowerControl(const std::string& node_id,
                                   const std::string& command) {
    std::shared_ptr<dbc::db_bare_metal> bm =
        BareMetalNodeManager::instance().getBareMetalNode(node_id);
    if (!bm) return FResult(ERR_ERROR, "node id not existed");

    std::string arg1 = command;
    if (command == "on" || command == "off") {
        arg1 = "power" + command;
    } else if (command != "status") {
        return FResult(ERR_ERROR, "unsupported command");
    }
    std::string arg2 = GetBareMetalInfo(bm);

    FResult fret = RunPowerCommand(power_manager_tool_, arg1, arg2);
    LOG_INFO << "run netbar power manager tool (" << power_manager_tool_ << " "
             << arg1 << " " << arg2 << ") return " << fret.errcode << " with "
             << fret.errmsg;
    return fret;
}

FResult PowerManager::SetBootDeviceOrder(const std::string& node_id,
                                         const std::string& device) {
    return FResult(ERR_ERROR, "unsupported command");
}

}  // namespace fool
