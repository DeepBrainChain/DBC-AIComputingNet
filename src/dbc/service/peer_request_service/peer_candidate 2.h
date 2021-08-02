#pragma once

#include "util/utils.h"
#include "peer_node.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "util/crypto/utilstrencodings.h"

namespace bf = boost::filesystem;
namespace rj = rapidjson;

#define DEFAULT_CONNECT_PEER_NODE      102400                            //default connect peer nodes
#define DAT_PEERS_FILE_NAME            "peers.dat"
#define MAX_PEER_CANDIDATES_CNT        1024

enum net_state
{
    ns_idle = 0,   //can use whenever needed
    ns_in_use,     //connecting or connected
    ns_failed,     //not use within a long time
    ns_zombie,
    ns_available
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
    peer_node_type node_type;

    peer_candidate()
        : net_st(ns_idle)
        , reconn_cnt(0)
        , last_conn_tm(time(nullptr))
        , score(0)
        , node_id("")
        , node_type(NORMAL_NODE)
    {
    }

    peer_candidate(tcp::endpoint ep, net_state ns = ns_idle, peer_node_type ntype = NORMAL_NODE, uint32_t rc_cnt = 0
        , time_t t = time(nullptr), uint32_t sc = 0, std::string nid = "")
        : tcp_ep(ep)
        , net_st(ns)
        , reconn_cnt(rc_cnt)
        , last_conn_tm(t)
        , score(sc)
        , node_id(nid)
        , node_type(ntype)
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
    case ns_available:
        return "available";
    default:
        return "";
    }
}

static std::string node_type_2_string(int8_t nt)
{
    switch((node_net_type) nt)
    {
        case nnt_normal_node:
            return "nnt_normal_node";
        case nnt_public_node:
            return "nnt_public_node";
        case nnt_super_node:
            return "nnt_super_node";
        default:
            return "";
    }
}
