#ifndef DBC_NET_DEF_H
#define DBC_NET_DEF_H

#include <string>
#include <vector>

#define MAIN_NET                             0xF1E1B0F9 //每次需要修改,ai:0xF1E1C0D9,fc1:0xF1E1B0E9
#define MAIN_NET_TYPE                        "mainnet"//ai:main5,fc:mainfc2
#define DEFAULT_LOG_LEVEL                    2

static const std::string DEFAULT_STRING = "";
static const std::string DEFAULT_LOCAL_IP = "0.0.0.0";
static const std::string DEFAULT_LOOPBACK_IP = "127.0.0.1";
static const std::string DEFAULT_IP_V4 = "127.0.0.1";

#endif //DBC_NET_DEF_H
