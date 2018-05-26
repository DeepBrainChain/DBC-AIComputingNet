/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name     : peer_candidate.h
* description   : peer candidates
* date          : 2018.05.09
* author        : Allan
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

        };

        static int32_t save_peer_candidates(std::list<peer_candidate> &cands)
        {
            if (cands.empty())
            {
                return E_DEFAULT;
            }
            
            try
            { 
                //serialize
                rj::Document document;
                rj::Document::AllocatorType& allocator = document.GetAllocator();
                rj::Value root(rj::kObjectType);
                rj::Value peer_cands(rj::kArrayType);
                for (auto it = cands.begin(); it != cands.end(); ++it)
                {
                    rj::Value peer_cand(rj::kObjectType);
                    std::string ip = it->tcp_ep.address().to_string();
                    rj::Value str_val(ip.c_str(), (rj::SizeType) ip.length(), allocator);
                    peer_cand.AddMember("ip", str_val, allocator);
                    peer_cand.AddMember("port", it->tcp_ep.port(), allocator);
                    peer_cand.AddMember("net_state", it->net_st, allocator);
                    peer_cand.AddMember("reconn_cnt", it->reconn_cnt, allocator);
                    peer_cand.AddMember("last_conn_tm", it->last_conn_tm, allocator);
                    peer_cand.AddMember("score", it->score, allocator);

                    peer_cands.PushBack(peer_cand, allocator);
                }
                root.AddMember("peer_cands", peer_cands, allocator);

                std::shared_ptr<rj::StringBuffer> buffer = std::make_shared<rj::StringBuffer>();
                rj::PrettyWriter<rj::StringBuffer> writer(*buffer);
                root.Accept(writer);

                //open file; if not exist, create it
                bf::path peers_file = bf::current_path();
                peers_file /= fs::path(DAT_DIR_NAME);
                peers_file /= fs::path(DAT_PEERS_FILE_NAME);
                if (matrix::core::file_util::write_file(peers_file, std::string(buffer->GetString())))
                    return E_SUCCESS;
            }
            catch (...)
            {
                return E_DEFAULT;
            }

            return E_DEFAULT;
        }

        static int32_t load_peer_candidates(std::list<peer_candidate> &cands)
        {
            cands.clear();

            std::string json_str;
            bf::path peers_file = bf::current_path();
            peers_file /= fs::path(DAT_DIR_NAME);
            peers_file /= fs::path(DAT_PEERS_FILE_NAME);
            if (!matrix::core::file_util::read_file(peers_file, json_str))
            {
                return E_FILE_FAILURE;
            }
            if (json_str.empty())
            {
                return E_DEFAULT;
            }

            try
            {
                rj::Document doc;
                doc.Parse<rj::kParseStopWhenDoneFlag>(json_str.c_str());
                //transfer to cands
                if (!doc.HasMember("peer_cands"))
                    return E_DEFAULT;
                rj::Value &val_arr = doc["peer_cands"];
                if (val_arr.IsArray())
                {
                    for (rj::SizeType i = 0; i < val_arr.Size(); i++)
                    {
                        const rj::Value& obj = val_arr[i];
                        peer_candidate peer_cand;
                        if (!obj.HasMember("ip"))
                            continue;
                        std::string ip = obj["ip"].GetString();
                        uint16_t port = obj["port"].GetUint();
                        tcp::endpoint ep(address_v4::from_string(ip), (uint16_t)port);
                        peer_cand.tcp_ep = ep;
                        peer_cand.net_st = (net_state) obj["net_state"].GetUint();
                        peer_cand.reconn_cnt = obj["reconn_cnt"].GetUint();
                        peer_cand.score = obj["score"].GetUint();

                        cands.push_back(peer_cand);
                    }
                }
            }
            catch (...)
            {
                LOG_ERROR << "read peers from " << peers_file.c_str() << "failed.";
                return E_DEFAULT;
            }
            
            return E_SUCCESS;
        }

    }
}