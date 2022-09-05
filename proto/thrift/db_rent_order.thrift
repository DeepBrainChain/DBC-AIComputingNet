namespace cpp dbc

struct db_rent_order {
    1: required string id,              // 租用订单
    2: required string renter,          // 租用人
    3: required i64 rent_end,           // 租用结束时间
	4: required list<i32> gpu_index,    // 租用的GPU序号
	5: required i32 gpu_num,            // 租用的GPU数量
	6: required string rent_status      // 租用状态
}
