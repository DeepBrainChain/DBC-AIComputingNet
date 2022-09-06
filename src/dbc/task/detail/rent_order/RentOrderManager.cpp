#include "RentOrderManager.h"
#include "log/log.h"
#include "task/HttpDBCChainClient.h"

RentOrderManager::RentOrderManager() = default;

RentOrderManager::~RentOrderManager() = default;

FResult RentOrderManager::Init() {
    bool ret = db_.init_db(EnvManager::instance().get_db_path(), "rent_order.db");
    if (!ret) {
        return FResult(ERR_ERROR, "init rent_order_db failed");
    }

    db_.load_datas(rent_orders_);
    return FResultOk;
}

void RentOrderManager::Exit() {

}

const RentOrder::RentOrderMap RentOrderManager::GetRentOrders() const {
    RwMutex::ReadLock rlock(mutex_);
    return rent_orders_;
}

void RentOrderManager::UpdateRentOrder(const std::string& machine_id,
        const std::string& rent_order) {
    auto pro = HttpDBCChainClient::instance().getRentOrder(machine_id, rent_order);
    if (pro) {
        {
            // 租用到期或未确认订单信息会发生变化，像rent_end会变成0。
            // 此时不应该更新，因为更新后无法确认到期n天后删除虚拟机。
            RwMutex::ReadLock rlock(mutex_);
            if (rent_orders_.count(pro->id) > 0) {
                auto old = rent_orders_[pro->id];
                if (pro->rent_end == 0) pro->__set_rent_end(old->rent_end);
                if (pro->gpu_index.empty()) pro->__set_gpu_index(old->gpu_index);
                if (pro->gpu_num == 0) pro->__set_gpu_num(old->gpu_num);
            }
        }
        RwMutex::WriteLock wlock(mutex_);
        if (rent_orders_.count(pro->id) == 0 || *rent_orders_[pro->id] != *pro) {
            std::string gpu_index;
            for (const auto& index : pro->gpu_index)
                gpu_index.append(" " + index);
            LOG_INFO << "update rent order, " << pro->id
                << ", rent_wallet: " << pro->renter
                << ", rent end: " << pro->rent_end
                << ", rent status: " << pro->rent_status
                << ", gpu num: " << pro->gpu_num
                << ", gpu index:" << gpu_index;
            rent_orders_[pro->id] = pro;
            db_.write_data(pro);
        }
    }
}

void RentOrderManager::UpdateRentOrders(const std::string& machine_id) {
    std::set<std::string> orders;
    HttpDBCChainClient::instance().getRentOrderList(machine_id, orders);
    for (const auto& order : orders) {
        UpdateRentOrder(machine_id, order);
    }
}

RentOrder::RentStatus RentOrderManager::GetRentStatus(const std::string& rent_order,
        const std::string& wallet) const {
    RwMutex::ReadLock rlock(mutex_);
    auto iter = rent_orders_.find(rent_order.empty() ? wallet : rent_order);
    if (iter == rent_orders_.end()) return RentOrder::RentStatus::None;
    if (iter->second->renter != wallet) return RentOrder::RentStatus::None;
    if (iter->second->rent_status == "rentExpired")
        return RentOrder::RentStatus::RentExpired;
    if (iter->second->rent_status == "renting" ||
            iter->second->rent_status == "waitingVerifying")
        return RentOrder::RentStatus::Renting;
}

uint64_t RentOrderManager::GetRentEnd(const std::string& rent_order,
        const std::string& wallet) const {
    RwMutex::ReadLock rlock(mutex_);
    auto iter = rent_orders_.find(rent_order.empty() ? wallet : rent_order);
    if (iter == rent_orders_.end()) return 0;
    if (iter->second->renter != wallet) return 0;
    return iter->second->rent_end;
}

std::vector<int32_t> RentOrderManager::GetRentedGpuIndex(const std::string& rent_order,
        const std::string& wallet) const {
    std::vector<int32_t> index;
    RwMutex::ReadLock rlock(mutex_);
    auto iter = rent_orders_.find(rent_order.empty() ? wallet : rent_order);
    if (iter == rent_orders_.end()) return index;
    if (iter->second->renter != wallet) return index;
    index = iter->second->gpu_index;
    return index;
}

std::set<std::string> RentOrderManager::GetRentingOrders() const {
    std::set<std::string> orders;
    RwMutex::ReadLock rlock(mutex_);
    for (const auto& iter : rent_orders_) {
        if (iter.second->rent_status == "renting" ||
            iter.second->rent_status == "waitingVerifying") {
            orders.insert(iter.first);
        }
    }
    return orders;
}

void RentOrderManager::ClearExpiredRentOrder(uint64_t cur_block) {
    if (cur_block == 0) return;
    std::vector<std::string> ids;
    {
        RwMutex::ReadLock rlock(mutex_);
        for (const auto& iter : rent_orders_) {
            if (iter.second->rent_status != "rentExpired") continue;
            if (cur_block - iter.second->rent_end > 120 * 24 * 30)
                ids.push_back(iter.first);
        }
    }
    RwMutex::WriteLock wlock(mutex_);
    for (const auto& id : ids) {
        if (db_.delete_data(id))
            rent_orders_.erase(id);
    }
}
