#include "BareMetalTaskManager.h"

#include "bare_metal/cloud_cybercafe_client.h"
#include "bare_metal/ipmitool_client.h"
#include "config/conf_manager.h"
#include "server/server.h"
#include "task/HttpDBCChainClient.h"
#include "task/bare_metal/bare_metal_node_manager.h"

BareMetalTaskManager::BareMetalTaskManager()
    : stopped_(false), bare_metal_client_(nullptr) {
    //
}

BareMetalTaskManager::~BareMetalTaskManager() {
    //
}

FResult BareMetalTaskManager::Init() {
    if (Server::NodeType != NODE_TYPE::BareMetalNode)
        return FResult(ERR_ERROR, "not a bare metal node");

    std::string cloud_cybercafe_server =
        ConfManager::instance().GetCloudCybercafeServer();
    if (cloud_cybercafe_server.empty())
        bare_metal_client_ = std::make_shared<IpmiToolClient>();
    else
        bare_metal_client_ = std::make_shared<CloudCybercafeClient>();
    return bare_metal_client_->Init();
}

void BareMetalTaskManager::Exit() {
    stopped_ = true;
    if (bare_metal_client_) bare_metal_client_->Exit();
    LOG_INFO << "BareMetalTaskManager exited";
}

FResult BareMetalTaskManager::PowerControl(const std::string& node_id,
                                           const std::string& command) {
    if (bare_metal_client_)
        return bare_metal_client_->PowerControl(node_id, command);
    return FResult(ERR_ERROR, "bare metal client not existed");
}

FResult BareMetalTaskManager::SetBootDeviceOrder(const std::string& node_id,
                                                 const std::string& device) {
    if (bare_metal_client_)
        bare_metal_client_->SetBootDeviceOrder(node_id, device);
    return FResult(ERR_ERROR, "bare metal client not existed");
}

void BareMetalTaskManager::PowerOffOnce(const std::string& node_id) {
    if (poweroff_once_.count(node_id) > 0 && !poweroff_once_[node_id]) {
        FResult fret = PowerControl(node_id, "off");
        if (fret.errcode == ERR_SUCCESS) {
            poweroff_once_[node_id] = true;
        }
    }
}

void BareMetalTaskManager::PruneNode(const std::string& node_id) {
    MACHINE_STATUS machine_status =
        HttpDBCChainClient::instance().request_machine_status(node_id);
    if (machine_status == MACHINE_STATUS::Unknown) return;

    std::string cur_renter;
    int64_t cur_rent_end = 0;
    HttpDBCChainClient::instance().request_cur_renter(node_id, cur_renter,
                                                      cur_rent_end);
    int64_t cur_block = HttpDBCChainClient::instance().request_cur_block();
    LOG_INFO << "prune node status is " << (int32_t)machine_status
             << ", current renter is " << cur_renter << ", rent end is "
             << cur_rent_end << ", cur block is " << cur_block;
    if (machine_status == MACHINE_STATUS::Online) {
        PowerOffOnce(node_id);
    } else if (machine_status == MACHINE_STATUS::Rented) {
        if (cur_rent_end > 0 && cur_block > cur_rent_end) {
            PowerOffOnce(node_id);
        } else {
            poweroff_once_[node_id] = false;
        }
    }
}

void BareMetalTaskManager::PruneNodes() {
    HttpDBCChainClient::instance().update_chain_order(
        ConfManager::instance().GetDbcChainDomain());

    std::vector<std::string> ids;
    {
        const std::map<std::string, std::shared_ptr<dbc::db_bare_metal>>
            bm_nodes = BareMetalNodeManager::instance().getBareMetalNodes();
        for (const auto& iter : bm_nodes) ids.push_back(iter.first);
    }

    for (const auto& id : ids) {
        if (poweroff_once_.count(id) == 0) poweroff_once_[id] = false;
        if (stopped_) break;
        PruneNode(id);
        sleep(3);
    }
}
