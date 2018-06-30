/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name           :   peer_candidate.h
* description       :   peer candidates
* date                     :   2018.05.09
* author                :   Allan
**********************************************************************************/
#pragma once

#include <string>
#include <list>
#include "peer_node.h"
#include "error.h"
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include "util.h"
#include "error/en.h"
#include "ip_validator.h"
#include "port_validator.h"
#include "base58.h"
#include "utilstrencodings.h"

using namespace std;
#ifdef WIN32
using namespace stdext;
#endif
using namespace matrix::core;
using namespace boost::asio::ip;
namespace bf = boost::filesystem;
namespace rj = rapidjson;


#define DEFAULT_CONNECT_PEER_NODE      102400                            //default connect peer nodes
#define DAT_PEERS_FILE_NAME            "peers.dat"
#define MAX_PEER_CANDIDATES_CNT        1024


namespace matrix
{
    namespace service_core
    {
        enum net_state
        {
            ns_idle = 0,   //can use whenever needed
            ns_in_use,     //connecting or connected
            ns_failed,     //not use within a long time
            //ns_discard,    //i.e: itself
            ns_zombie
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
            std::string     node_id;

            peer_candidate()
                : net_st(ns_idle)
                , reconn_cnt(0)
                , last_conn_tm(time(nullptr))
                , score(0)
                , node_id("")
            {
            }

            peer_candidate(tcp::endpoint ep, net_state ns = ns_idle, uint32_t rc_cnt = 0
                , time_t t = time(nullptr), uint32_t sc = 0, std::string nid = "")
                : tcp_ep(ep)
                , net_st(ns)
                , reconn_cnt(rc_cnt)
                , last_conn_tm(t)
                , score(sc)
                , node_id(nid)
            {
            }
        };

        static std::string net_state_2_string(int8_t st)
        {
            switch ((net_state)st)
            {
            case ns_idle:
                return "idle";
            case ns_in_use:
                return "in_use";
            case ns_failed:
                return "failed";
            case ns_zombie:
                return "zombie";
            default:
                return "";
            }
        }

    }
}