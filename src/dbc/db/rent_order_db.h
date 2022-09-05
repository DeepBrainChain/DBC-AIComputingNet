#ifndef DBC_RENT_ORDER_DB_H
#define DBC_RENT_ORDER_DB_H

#include "util/utils.h"
#include <leveldb/db.h>
#include <boost/filesystem.hpp>
#include "db_types/db_rent_order_types.h"

class RentOrderDB final
{
public:
    RentOrderDB() = default;
    ~RentOrderDB() = default;

    bool init_db(boost::filesystem::path db_path, std::string db_name);

    void load_datas(std::map<std::string, std::shared_ptr<dbc::db_rent_order>>& orders);

    std::shared_ptr<dbc::db_rent_order> read_data(const std::string& order_id);

    bool write_data(const std::shared_ptr<dbc::db_rent_order>& order);

	bool delete_data(const std::string& order_id);

protected:
    std::unique_ptr<leveldb::DB> m_db;
};

#endif  // DBC_RENT_ORDER_DB_H
