namespace cpp dbc

struct db_wallet_renttask {
    1: required string rent_wallet,
    2: required list<string> task_ids,
    3: required i64 rent_end
}
