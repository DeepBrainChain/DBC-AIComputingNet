/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   net_address.cpp
* description      :   p2p network address
* date             :   2018.07.02
* author           :   Bruce Feng
**********************************************************************************/

#include "net_address.h"


namespace matrix
{
    namespace core
    {

        bool net_address::is_rfc1918(tcp::endpoint tcp_ep)
        {
            if (tcp_ep.address().is_v4())
            {
                //10.x.x.x or 192.168.x.x or 172.[16-31].x.x
                address_v4::bytes_type bytes = tcp_ep.address().to_v4().to_bytes();
                if (bytes[0] == 10
                    || (bytes[1] == 168 && bytes[0] == 192)
                    || ((bytes[1] >= 16 && bytes[1] <= 31) && bytes[0] == 172))
                {
                    return true;
                }
            }

            return false;
        }

    }

}