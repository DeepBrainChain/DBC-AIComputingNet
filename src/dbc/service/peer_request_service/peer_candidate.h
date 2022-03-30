#ifndef PEER_CANDIDATE_H
#define PEER_CANDIDATE_H

#include "util/utils.h"
#include "peer_node.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "util/crypto/utilstrencodings.h"
#include <boost/asio.hpp>

namespace bfs = boost::filesystem;
using namespace boost::asio;

#define DEFAULT_CONNECT_PEER_NODE      102400       //default connect peer nodes
#define DAT_PEERS_FILE_NAME            "peers.dat"

enum net_state
{
    ns_idle = 0,   //can use whenever needed
    ns_in_use,     //connecting or connected
    ns_failed,     //not use within a long time
    ns_zombie,
    ns_available
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

struct peer_candidate
{
    ip::tcp::endpoint   tcp_ep;
    peer_node_type  node_type = NORMAL_NODE;
    std::string     node_id;
    net_state       net_st = ns_idle;
    uint32_t        reconn_cnt = 0;
    time_t          last_conn_tm;
    uint32_t        score = 0;  //indicate level of Qos, update when disconnect
    
    peer_candidate() {
        last_conn_tm = time(nullptr);
    }

    peer_candidate(ip::tcp::endpoint ep, net_state _net_state = ns_idle, 
        peer_node_type _peer_node_type = NORMAL_NODE, uint32_t _reconn_cnt = 0, 
        time_t _last_conn_tm = time(nullptr), uint32_t _score = 0, std::string _node_id = "")
        : tcp_ep(ep)
        , net_st(_net_state)
        , reconn_cnt(_reconn_cnt)
        , last_conn_tm(_last_conn_tm)
        , score(_score)
        , node_id(_node_id)
        , node_type(_peer_node_type)
    {

    }
};

#endif
