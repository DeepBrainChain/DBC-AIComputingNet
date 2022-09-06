#ifndef DBC_RENT_ORDER_DB_H
#define DBC_RENT_ORDER_DB_H

#include "util/utils.h"
#include <leveldb/db.h>
#include <boost/filesystem.hpp>
#include "db_types/db_rent_order_types.h"

namespace RentOrder {

struct Compare {
    bool operator() (const std::string& key1, const std::string& key2) const;
};

typedef std::map<std::string, std::shared_ptr<dbc::db_rent_order>, RentOrder::Compare> RentOrderMap;

}  // namespace RentOrder

class RentOrderDB final
{
public:
    RentOrderDB() = default;
    ~RentOrderDB() = default;

    bool init_db(boost::filesystem::path db_path, std::string db_name);

    void load_datas(RentOrder::RentOrderMap& orders);

    std::shared_ptr<dbc::db_rent_order> read_data(const std::string& order_id);

    bool write_data(const std::shared_ptr<dbc::db_rent_order>& order);

	bool delete_data(const std::string& order_id);

protected:
    std::unique_ptr<leveldb::DB> m_db;
};

#endif  // DBC_RENT_ORDER_DB_H
