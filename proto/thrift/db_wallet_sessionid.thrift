namespace cpp dbc

struct db_wallet_sessionid {
    1: required string rent_wallet,
    2: required string session_id,
    3: optional list<string> multisig_signers
}
