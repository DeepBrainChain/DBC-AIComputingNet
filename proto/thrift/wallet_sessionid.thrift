namespace cpp dbc

struct rent_sessionid {
    1: required string rent_wallet,
    2: required string session_id,
    3: optional list<string> multisig_signers
}
