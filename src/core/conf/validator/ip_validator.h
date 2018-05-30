/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   ip_validator.h
* description    :   ip validator for config parameter
* date                  :   2018.03.21
* author            :   Bruce Feng
**********************************************************************************/
#pragma once


#include "util.h"
#include "common.h"
#include "conf_validator.h"
#include <boost/exception/all.hpp>
#include <boost/asio/ip/network_v4.hpp>
#include <algorithm>

#define IP_V4_FIELDS_COUNT                           4
#define MAX_IP_V6_LEN                                    39                 //FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF
#define MIN_IP_V6_LEN                                       2                  //::
#define MAX_CHARACTERS_IN_FIELD            4
#define MAX_SINGLE_COLON_COUNT             7


namespace matrix
{
    namespace core
    {

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
                    
                    transform(ip.begin(), ip.end(), ip.begin(), static_cast<int(*)(int)>(tolower));
                    transform(ip_addr.begin(), ip_addr.end(), ip_addr.begin(), static_cast<int(*)(int)>(tolower));

                    return ((addr.is_v4() || addr.is_v6()) && (ip_addr == ip));
                }
                catch (const boost::exception & e)
                {
                    LOG_ERROR << "ip_validator validate exception: " << diagnostic_information(e);
                    return false;
                }
            }

            //bool is_ipv4(const std::string& ip)
            //{
            //    std::vector<std::string> vec;
            //    string_util::split(ip, ".", vec);

            //    //not 4 fields
            //    if (IP_V4_FIELDS_COUNT != vec.size())
            //    {
            //        return false;
            //    }

            //    int i = 0;
            //    while (i < vec.size())
            //    {
            //        std::string ip_field = vec[i++];

            //        try
            //        {
            //            unsigned long ul_ip_field = stoul(ip_field);
            //            if (ul_ip_field > 255)
            //            {
            //                return false;
            //            }
            //        }
            //        catch (const std::exception &e)
            //        {
            //            LOG_ERROR << "is_ipv4 caught exception: " << e.what();
            //            return false;
            //        }
            //    }

            //    return true;
            //}

            //bool is_ipv6(const std::string& ip)
            //{

            //    //FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF
            //    //1050:0:0:0:5:600:300c:326b
            //    //::
            //    //ff06::c3

            //    int double_colons_count = 0;                 //::
            //    int single_colon_count = 0;                   //0:0

            //    int i = 0;
            //    size_t ip_len = ip.length();

            //    //FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF  or  ::
            //    if (ip_len < MIN_IP_V6_LEN || ip_len > MAX_IP_V6_LEN)
            //    {
            //        return false;
            //    }

            //    if ((ip[0] == ':' && ip[1] != ':')
            //        || (ip[ip_len - 1] == ':' && ip[ip_len - 2] != ':'))
            //    {
            //        return false;
            //    }

            //    int characters_in_field = 0;
            //    for (size_t i = 0; i < ip_len; i++)
            //    {
            //        //valid character
            //        if (!((ip[i] >= '0' && ip[i] <= '9') || (ip[i] >= 'a' && ip[i] <= 'f') || (ip[i] >= 'A' && ip[i] <= 'F') || (ip[i] == ':')))
            //        {
            //            return false;
            //        }

            //        //:: times
            //        if ((i < (ip_len - 1)) && ip[i] == ':' && ip[i + 1] == ':')
            //        {
            //            double_colons_count++;

            //            //more than one time
            //            if (double_colons_count > 1)
            //            {
            //                return false;
            //            }
            //        }

            //        if (ip[i] != ':')
            //        {
            //            characters_in_field++;
            //            if (characters_in_field > MAX_CHARACTERS_IN_FIELD)
            //            {
            //                return false;
            //            }
            //        }
            //        else
            //        {
            //            characters_in_field = 0;
            //        }

            //        if ((i > 0) && (i < ip_len - 1) && (ip[i - 1] != ':') && (ip[i] == ':') && (ip[i + 1] != ':'))
            //        {
            //            single_colon_count++;
            //            if (single_colon_count > MAX_SINGLE_COLON_COUNT)
            //            {
            //                return false;
            //            }
            //        }

            //    }

            //    return true;
            //}

        };

    }

}
