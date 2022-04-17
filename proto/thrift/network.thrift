namespace cpp dbc

//////////////////////////////////////////////////////////////////////////
struct networkInfo {
  1: required string networkId,
  2: required string bridgeName,
  3: required string vxlanName,
  4: required string vxlanVni,
  5: required string ipCidr,
  6: required string ipStart,
  7: required string ipEnd,
  8: optional string dhcpInterface,
  12: optional string machineId,
  13: optional string rentWallet,
  14: optional list<string> members,
  15: optional i64 lastUseTime,
  16: optional i32 nativeFlags
}
