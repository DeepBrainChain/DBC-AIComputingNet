/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name     peer_candidate.h
* description   peer candidates
* date          2018.05.09
* author        Allan
**********************************************************************************/
#pragma once

#include <string>
#include <list>
#include "peer_node.h"

using namespace std;
#ifdef WIN32
using namespace stdext;
#endif
using namespace matrix::core;
using namespace boost::asio::ip;

#define DEFAULT_CONNECT_PEER_NODE      102400                            //default connect peer nodes

namespace matrix
{
    namespace service_core
    {
        enum net_state
        {
            ns_idle = 0,   //can use whenever needed
            ns_in_use,     //connecting or connected
            ns_failed      //not use within a long time
        };
        enum node_net_type
        {
            nnt_normal_node = 0,    //nat1-4
            nnt_public_node,        //nat0
            nnt_super_node,         //
        };
        struct peer_candidate
        {
            tcp::endpoint   tcp_ep;
            net_state       net_st;
            uint32_t        reconn_cnt;
            time_t          last_conn_tm;
            uint32_t        score;//indicate level of Qos, update when disconnect
            //node_net_type   node_type;
        };
    }
}