#ifndef DBC_IP_VALIDATOR_H
#define DBC_IP_VALIDATOR_H

#include "common/common.h"
#include "conf_validator.h"
#include <boost/exception/all.hpp>
#include <boost/asio/ip/network_v4.hpp>
#include <algorithm>

#define IP_V4_FIELDS_COUNT                           4
#define MAX_IP_V6_LEN                                    39                 //FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF
#define MIN_IP_V6_LEN                                       2                  //::
#define MAX_CHARACTERS_IN_FIELD            4
#define MAX_SINGLE_COLON_COUNT             7

class ip_validator : public conf_validator
{
public:
    //modify by regulus:fix can't validate ipaddr contain "-0", use boost::asio::ip::address to validate error.
    bool validate(const variable_value &val)
    {
        try
        {
            std::string ip = val.as<std::string>();
            boost::asio::ip::address addr = boost::asio::ip::make_address(ip);
            std::string ip_addr = addr.to_string();

            transform(ip.begin(), ip.end(), ip.begin(), ::tolower);
            transform(ip_addr.begin(), ip_addr.end(), ip_addr.begin(), ::tolower);

            return ((addr.is_v4() || addr.is_v6()) && (ip_addr == ip));
        }
        catch (const boost::exception & e)
        {
            //LOG_ERROR << "ip_validator validate exception: " << diagnostic_information(e);
            return false;
        }
    }
};

#endif
