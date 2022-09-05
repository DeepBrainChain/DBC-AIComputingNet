#ifndef DBC_RENT_ORDER_MANAGER_H
#define DBC_RENT_ORDER_MANAGER_H

#include "util/utils.h"
#include "db/rent_order_db.h"

namespace RentOrder {

enum RentStatus {
    None,
    Renting,
    RentExpired
};

}  // namespace RentOrder

class RentOrderManager : public Singleton<RentOrderManager> {
public:
    RentOrderManager();
    virtual ~RentOrderManager();

    typedef std::map<std::string, std::shared_ptr<dbc::db_rent_order>> RentOrderMap;

    FResult Init();

    void Exit();

    const RentOrderMap GetRentOrders() const;

    void UpdateRentOrder(const std::string& machine_id, const std::string& rent_order);

    void UpdateRentOrders(const std::string& machine_id);

    RentOrder::RentStatus GetRentStatus(const std::string& rent_order,
        const std::string& wallet) const;

    uint64_t GetRentEnd(const std::string& id, const std::string& wallet) const;

    std::vector<int32_t> GetRentedGpuIndex(const std::string& id,
        const std::string& wallet) const;

    std::set<std::string> GetRentingOrders() const;

    void ClearExpiredRentOrder(uint64_t cur_block);

private:
    mutable RwMutex mutex_;
    RentOrderMap rent_orders_;
    RentOrderDB db_;
};

#endif  // DBC_RENT_ORDER_MANAGER_H
