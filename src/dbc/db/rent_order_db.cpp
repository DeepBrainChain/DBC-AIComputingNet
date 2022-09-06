#include "rent_order_db.h"
#include "log/log.h"
#include "util/memory/byte_buf.h"
#include "network/protocol/thrift_binary.h"

namespace RentOrder {

// 订单ID全是数字，查到的其实是uint64类型
// 订单ID在钱包地址的前面
// 订单ID按数字从大到小排列
bool Compare::operator() (const std::string& key1, const std::string& key2) const {
    if (key1.length() == 48 && key2.length() == 48) return key1 < key2;
    else if (key1.length() == 48) return false;
    else if (key2.length() == 48) return true;
    int64_t num1 = atoll(key1.c_str());
    int64_t num2 = atoll(key2.c_str());
    return num1 > num2;
}

}  // namespace RentOrder

bool RentOrderDB::init_db(boost::filesystem::path db_path, std::string db_name) {
    leveldb::DB* db = nullptr;
    leveldb::Options  options;
    options.create_if_missing = true;

    if (!bfs::exists(db_path))
    {
        LOG_INFO << "db directory path does not exist and create db directory" << db_path;
        bfs::create_directory(db_path);
    }

    if (!bfs::is_directory(db_path))
    {
        LOG_INFO << "db directory path does not exist and exit" << db_path;
        return false;
    }

    db_path /= bfs::path(db_name);

    leveldb::Status status = leveldb::DB::Open(options, db_path.generic_string(), &db);
    if (!status.ok())
    {
        LOG_ERROR << "open db error: " << status.ToString();
        return false;
    }

    m_db.reset(db);

    return true;
}

void RentOrderDB::load_datas(RentOrder::RentOrderMap& orders) {
    std::unique_ptr<leveldb::Iterator> it;
    it.reset(m_db->NewIterator(leveldb::ReadOptions()));
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        std::shared_ptr<dbc::db_rent_order> order = std::make_shared<dbc::db_rent_order>();
        std::shared_ptr<byte_buf> order_buf = std::make_shared<byte_buf>();
        order_buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());
        network::binary_protocol proto(order_buf.get());
        order->read(&proto);

        orders.insert({ order->id, order });
    }
}

std::shared_ptr<dbc::db_rent_order> RentOrderDB::read_data(const std::string& order_id) {
    if (order_id.empty())
        return nullptr;

    std::string val;
    leveldb::Status status = m_db->Get(leveldb::ReadOptions(), order_id, &val);
    if (status.ok()) {
        std::shared_ptr<dbc::db_rent_order> order = std::make_shared<dbc::db_rent_order>();
        std::shared_ptr<byte_buf> order_buf = std::make_shared<byte_buf>();
        order_buf->write_to_byte_buf(val.c_str(), val.size());
        network::binary_protocol proto(order_buf.get());
        order->read(&proto);
        return order;
    }
    else {
        return nullptr;
    }
}

bool RentOrderDB::write_data(const std::shared_ptr<dbc::db_rent_order>& order) {
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    order->write(&proto);

    leveldb::WriteOptions write_options;
    write_options.sync = true;
    leveldb::Slice slice(out_buf->get_read_ptr(), out_buf->get_valid_read_len());
    leveldb::Status status = m_db->Put(write_options, order->id, slice);
    if (status.ok()) {
        return true;
    }
    else {
        LOG_INFO << "put data failed: " << order->id;
        return false;
    }
}

bool RentOrderDB::delete_data(const std::string& order_id) {
    if (order_id.empty())
		return true;

	leveldb::WriteOptions write_options;
	write_options.sync = true;
	leveldb::Status status = m_db->Delete(write_options, order_id);
	if (status.ok()) {
		return true;
	}
	else {
		return false;
	}
}
