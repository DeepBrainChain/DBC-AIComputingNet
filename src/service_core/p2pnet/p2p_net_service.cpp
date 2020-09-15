/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name           :    p2p_net_service.cpp
* description       :     p2p net service
* date                    :    2018.01.28
* author               :    Bruce Feng
**********************************************************************************/
#include "p2p_net_service.h"
#include <cassert>
#include "server.h"
#include "conf_manager.h"
#include "tcp_acceptor.h"
#include "service_message_id.h"
#include "service_message_def.h"
#include "matrix_types.h"
#include "matrix_client_socket_channel_handler.h"
#include "matrix_server_socket_channel_handler.h"

#include "channel.h"
#include "ip_validator.h"
#include "port_validator.h"
#include "api_call.h"
#include "id_generator.h"
#include "common/version.h"
#include "tcp_socket_channel.h"
#include "timer_def.h"
#include "peer_seeds.h"
#include "peers_db_types.h"
#include <leveldb/write_batch.h>
#include <boost/format.hpp>

#include "ai_crypter.h"

using namespace std;
using namespace matrix::core;
using namespace matrix::service_core;

const uint32_t max_reconnect_times = 1;
//const uint32_t max_connected_cnt = 8;
const uint32_t max_connected_cnt =MAX_OUTBOUND_CONNECTIONS;

const uint32_t max_connect_per_check = 16;


namespace matrix
{
    namespace service_core
    {
        p2p_net_service::p2p_net_service()
            : m_timer_check_peer_candidates(INVALID_TIMER_ID)
            , m_timer_dyanmic_adjust_network(INVALID_TIMER_ID)
            , m_timer_peer_info_exchange(INVALID_TIMER_ID)
            , m_timer_dump_peer_candidates(INVALID_TIMER_ID)
        {

        }

        void p2p_net_service::get_all_peer_nodes(peer_list_type &nodes)
        {
            for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); it++)
            {
                nodes.push_back(it->second);
            }
        }

        std::shared_ptr<peer_node> p2p_net_service::get_peer_node(const std::string &id)
        {
            auto it = m_peer_nodes_map.find(id);
            if (it == m_peer_nodes_map.end())
            {
                return nullptr;
            }

            return it->second;
        }

        int32_t p2p_net_service::init_rand()
        {
            m_rand_seed = uint256();
            m_rand_ctx = FastRandomContext(m_rand_seed);

            return E_SUCCESS;
        }

        int32_t p2p_net_service::init_conf()
        {
            variable_value val;
            ip_validator ip_vdr;
            port_validator port_vdr;

            //get listen ip and port conf
            const std::string & host_ip = CONF_MANAGER->get_host_ip();
            val.value() = host_ip;

            if (!ip_vdr.validate(val))
            {
                LOG_ERROR << "p2p_net_service init_conf invalid host ip: " << host_ip;
                return E_DEFAULT;
            }
            else
            {
                m_host_ip = host_ip;
            }

            std::string s_port = CONF_MANAGER->get_net_listen_port();
            val.value() = s_port;
            if (false == port_vdr.validate(val))
            {
                LOG_ERROR << "p2p_net_service init_conf invalid net port: " << s_port;
                return E_DEFAULT;
            }
            else
            {
                try
                {
                    m_net_listen_port = (uint16_t)std::stoi(s_port);
                }
                catch (const std::exception &e)
                {
                    LOG_ERROR << "p2p_net_service init_conf invalid main_port: " << s_port << ", " << e.what();
                    return E_DEFAULT;
                }
            }

            //dns seeds
            m_dns_seeds.insert(m_dns_seeds.begin(), CONF_MANAGER->get_dns_seeds().begin(), CONF_MANAGER->get_dns_seeds().end());

            //hard_code_seeds
            m_hard_code_seeds.insert(m_hard_code_seeds.begin(), CONF_MANAGER->get_hard_code_seeds().begin(), CONF_MANAGER->get_hard_code_seeds().end());

            return E_SUCCESS;
        }

        int32_t p2p_net_service::init_db()
        {
            leveldb::DB *db = nullptr;
            leveldb::Options options;
            options.create_if_missing = true;
            try
            {
                //get db path
                fs::path task_db_path = env_manager::get_db_path();
                if (false == fs::exists(task_db_path))
                {
                    LOG_DEBUG << "db directory path does not exist and create db directory";
                    fs::create_directory(task_db_path);
                }

                //check db directory
                if (false == fs::is_directory(task_db_path))
                {
                    LOG_ERROR << "db directory path does not exist and exit";
                    return E_DEFAULT;
                }

                task_db_path /= fs::path("peers.db");
                LOG_DEBUG << "peers db path: " << task_db_path.generic_string();

                //open db
                leveldb::Status status = leveldb::DB::Open(options, task_db_path.generic_string(), &db);
                if (false == status.ok())
                {
                    LOG_ERROR << "p2p net service init peers db error: " << status.ToString();
                    return E_DEFAULT;
                }

                //smart point auto close db
                m_peers_db.reset(db);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "create p2p db error: " << e.what();
                return E_DEFAULT;
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "create p2p db error" << diagnostic_information(e);
                return E_DEFAULT;
            }

            
            return E_SUCCESS;
        }

        int32_t p2p_net_service::init_acceptor()
        {
            //ipv4 or ipv6
            tcp::endpoint ep(ip::address::from_string(m_host_ip), m_net_listen_port);

            int32_t ret = E_SUCCESS;

            //net
            LOG_INFO << "p2p net service init net, ip: " << m_host_ip << " port: " << m_net_listen_port;
            ret = CONNECTION_MANAGER->start_listen(ep, &matrix_server_socket_channel_handler::create);
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "p2p net service init net error, ip: " << m_host_ip << " port: " << m_net_listen_port;
                return ret;
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::init_connector(bpo::variables_map &options)
        {
            //peer address from command line input
            //--peer 117.30.51.196:11107 --peer 1050:0:0:0:5:600:300c:326b:11107
            const std::vector<std::string> & cmd_addresses = options.count("peer") ? options["peer"].as<std::vector<std::string>>() : DEFAULT_VECTOR;
            
            //peer address from peer.conf
            //peer address=117.30.51.196:11107
            //peer address=1050:0:0:0:5:600:300c:326b:11107
            const std::vector<std::string> & peer_conf_addresses = CONF_MANAGER->get_peers();

            //merge peer addresses
            std::vector<std::string> peer_addresses; //(cmd_addresses.size() + peer_conf_addresses.size());

            peer_addresses.insert(peer_addresses.begin(), peer_conf_addresses.begin(), peer_conf_addresses.end());
            peer_addresses.insert(peer_addresses.begin(), cmd_addresses.begin(), cmd_addresses.end());

            int count = 0;
            ip_validator ip_vdr;
            port_validator port_vdr;

            //set up connection
            for (auto it = peer_addresses.begin(); it != peer_addresses.end() && count <DEFAULT_CONNECT_PEER_NODE; it++)
            {
                std::string addr = *it;
                string_util::trim(addr);
                size_t pos = addr.find_last_of(':');
                if (pos == std::string::npos)
                {
                    LOG_ERROR << "p2p net conf file invalid format: " << addr;
                    continue;
                }

                //get ip and port
                std::string ip = addr.substr(0, pos);
                std::string str_port = addr.substr(pos + 1, std::string::npos);

                //validate ip
                variable_value val;
                val.value() = ip;
                if (false == ip_vdr.validate(val))
                {
                    LOG_ERROR << "p2p_net_service init_connect invalid ip: " << ip;
                    continue;
                }

                //validate port
                if (str_port.empty())
                {
                    LOG_ERROR << "p2p_net_service init_connect invalid port: " << str_port;
                    continue;
                }                

                val.value() = str_port;
                if (false == port_vdr.validate(val))
                {
                    LOG_ERROR << "p2p_net_service init_connect invalid port: " << str_port;
                    continue;
                }

                uint16_t port = 0;
                try
                {
                    port = (uint16_t)std::stoi(str_port);
                }
                catch (const std::exception &e)
                {
                    LOG_ERROR << "p2p_net_service init_connect invalid port: " << str_port << ", " << e.what();
                    continue;
                }

                try
                {
                    tcp::endpoint ep(ip::address::from_string(ip), (uint16_t)port);
                    
                    //start connect
                    LOG_DEBUG << "matrix connect peer address, ip: " << ip << " port: " << str_port;
                    if (exist_peer_node(ep))
                    {
                        LOG_DEBUG << "tcp channel exist to: " << ep.address().to_string();
                        continue;
                    }
                    
                    int32_t ret = CONNECTION_MANAGER->start_connect(ep, &matrix_client_socket_channel_handler::create);                   
                    if (E_SUCCESS != ret)
                    {
                        LOG_ERROR << "matrix init connector invalid peer address, ip: " << ip << " port: " << str_port;
                        continue;
                    }
                    
                    if (is_peer_candidate_exist(ep))
                    {
                        //case: duplicated address from peer_addresses
                        update_peer_candidate_state(ep, ns_in_use);
                    }
                    else
                    {
                        add_peer_candidate(ep, ns_in_use, NORMAL_NODE);
                    }
                }
                catch (const std::exception &e)
                {
                    LOG_ERROR << "p2p_net_service init_connect abnormal. addr info: " << ip <<", port:"<<str_port << ", " << e.what();
                    continue;
                }

                count++;
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::service_init(bpo::variables_map &options)
        {
            int32_t ret = E_SUCCESS;

            if (E_SUCCESS != init_rand())
            {
                LOG_ERROR << "p2p_net_service init rand error and exit";
                return E_DEFAULT;
            }

            //init ip and port
            if (E_SUCCESS != init_conf())
            {
                LOG_ERROR << "p2p_net_service init acceptor error and exit";
                return E_DEFAULT;
            }

            //init db
            if (E_SUCCESS != init_db())
            {
                LOG_ERROR << "p2p_net_service init db error and exit";
                return E_DEFAULT;
            }

            //load peer candidates
            ret = load_peer_candidates();
            if (E_SUCCESS != ret)
            {
                LOG_WARNING << "load candidate peers failed.";
            }

            //init listen
            ret = init_acceptor();
            if (E_SUCCESS != ret)
            {
                return ret;
            }

            //init connect
            init_connector(options);
            if (E_SUCCESS != ret)
            {
                return ret;
            }

            return E_SUCCESS;
        }

        void p2p_net_service::init_subscription()
        {
            TOPIC_MANAGER->subscribe(TCP_CHANNEL_ERROR, [this](std::shared_ptr<message> &msg) {return send(msg);});
            TOPIC_MANAGER->subscribe(CLIENT_CONNECT_NOTIFICATION, [this](std::shared_ptr<message> &msg) {return send(msg);});
            TOPIC_MANAGER->subscribe(VER_REQ, [this](std::shared_ptr<message> &msg) {return send(msg);});
            TOPIC_MANAGER->subscribe(VER_RESP, [this](std::shared_ptr<message> &msg) {return send(msg);});
            TOPIC_MANAGER->subscribe(typeid(matrix::service_core::cmd_get_peer_nodes_req).name(), [this](std::shared_ptr<message> &msg) { return send(msg); });
            TOPIC_MANAGER->subscribe(P2P_GET_PEER_NODES_REQ, [this](std::shared_ptr<message> &msg) { return send(msg); });
            TOPIC_MANAGER->subscribe(P2P_GET_PEER_NODES_RESP, [this](std::shared_ptr<message> &msg) { return send(msg); });
        }

        void p2p_net_service::init_invoker()
        {
            invoker_type invoker;

            //tcp channel error
            invoker = std::bind(&p2p_net_service::on_tcp_channel_error, this, std::placeholders::_1);
            m_invokers.insert({ TCP_CHANNEL_ERROR,{ invoker } });

            //client tcp connect success
            invoker = std::bind(&p2p_net_service::on_client_tcp_connect_notification, this, std::placeholders::_1);
            m_invokers.insert({ CLIENT_CONNECT_NOTIFICATION,{ invoker } });

            //ver req
            invoker = std::bind(&p2p_net_service::on_ver_req, this, std::placeholders::_1);
            m_invokers.insert({ VER_REQ,{ invoker } });

            //ver resp
            invoker = std::bind(&p2p_net_service::on_ver_resp, this, std::placeholders::_1);
            m_invokers.insert({ VER_RESP,{ invoker } });

            //get_peer_nodes_req
            invoker = std::bind(&p2p_net_service::on_get_peer_nodes_req, this, std::placeholders::_1);
            m_invokers.insert({ P2P_GET_PEER_NODES_REQ,{ invoker } });

            //get_peer_nodes_resp
            invoker = std::bind(&p2p_net_service::on_get_peer_nodes_resp, this, std::placeholders::_1);
            m_invokers.insert({ P2P_GET_PEER_NODES_RESP,{ invoker } });

            //cmd_get_peer_nodes_req
            invoker = std::bind(&p2p_net_service::on_cmd_get_peer_nodes_req, this, std::placeholders::_1);
            m_invokers.insert({ typeid(matrix::service_core::cmd_get_peer_nodes_req).name(),{ invoker } });
        }

        void p2p_net_service::init_timer()
        {          
            m_timer_invokers[TIMER_NAME_CHECK_PEER_CANDIDATES] = std::bind(&p2p_net_service::on_timer_check_peer_candidates, this, std::placeholders::_1);
            add_timer(TIMER_NAME_CHECK_PEER_CANDIDATES, 1 * 5 * 1000, 1);
            m_timer_check_peer_candidates = add_timer(TIMER_NAME_CHECK_PEER_CANDIDATES, 1 *60 * 1000);
            assert(m_timer_check_peer_candidates != INVALID_TIMER_ID);

            m_timer_invokers[TIMER_NAME_DYANMIC_ADJUST_NETWORK] = std::bind(&p2p_net_service::on_timer_dyanmic_adjust_network, this, std::placeholders::_1);
            m_timer_dyanmic_adjust_network = add_timer(TIMER_NAME_DYANMIC_ADJUST_NETWORK, 1 * 60 * 1000);
            assert(m_timer_dyanmic_adjust_network != INVALID_TIMER_ID);

            m_timer_invokers[TIMER_NAME_PEER_INFO_EXCHANGE] = std::bind(&p2p_net_service::on_timer_peer_info_exchange, this, std::placeholders::_1);
            m_timer_peer_info_exchange = add_timer(TIMER_NAME_PEER_INFO_EXCHANGE, 3 * 60 * 1000);
            assert(m_timer_peer_info_exchange != INVALID_TIMER_ID);

            m_timer_invokers[TIMER_NAME_DUMP_PEER_CANDIDATES] = std::bind(&p2p_net_service::on_timer_peer_candidate_dump, this, std::placeholders::_1);
            m_timer_dump_peer_candidates = add_timer(TIMER_NAME_DUMP_PEER_CANDIDATES, 10 * 60 * 1000);
            assert(m_timer_dump_peer_candidates != INVALID_TIMER_ID);
        }

        int32_t p2p_net_service::service_exit()
        {
            if (m_timer_check_peer_candidates != INVALID_TIMER_ID)
            {
                remove_timer(m_timer_check_peer_candidates);
                m_timer_check_peer_candidates = INVALID_TIMER_ID;
            }

            if (m_timer_dyanmic_adjust_network != INVALID_TIMER_ID)
            {
                remove_timer(m_timer_dyanmic_adjust_network);
                m_timer_dyanmic_adjust_network = INVALID_TIMER_ID;
            }

            if (m_timer_peer_info_exchange != INVALID_TIMER_ID)
            {
                remove_timer(m_timer_peer_info_exchange);
                m_timer_peer_info_exchange = INVALID_TIMER_ID;
            }

            if (m_timer_dump_peer_candidates != INVALID_TIMER_ID)
            {
                remove_timer(m_timer_dump_peer_candidates);
                m_timer_dump_peer_candidates = INVALID_TIMER_ID;
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_timer_check_peer_candidates(std::shared_ptr<matrix::core::core_timer> timer)
        {
//            assert(timer->get_timer_id() == m_timer_check_peer_candidates);

            //clear ns failed status candidates
            for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end(); )
            {
                if (ns_failed == (*it)->net_st && (*it)->reconn_cnt >= max_reconnect_times)
                {
                    LOG_DEBUG << "remove peer candidate: " << (*it)->node_id << ", state= " << net_state_2_string((*it)->net_st);
                    m_peer_candidates.erase(it++);
                    continue;
                }
                it++;
            }

            //use dns peer seeds 
            if (get_maybe_available_peer_candidates_count() < MIN_PEER_CANDIDATES_COUNT)
            {
                add_dns_seeds();
            }

            //use hard code peer seeds 
            if (get_maybe_available_peer_candidates_count() < MIN_PEER_CANDIDATES_COUNT)       //still no available candidate
            {
                add_hard_code_seeds();
            }

            //check connections
            uint32_t new_conn_cnt = 0;
            for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end(); ++it)
            {
                //not too many conn at a time
                if (new_conn_cnt > max_connect_per_check)
                {
                    break;
                }

                std::shared_ptr<peer_candidate> candidate = *it;

                if ((ns_idle == candidate->net_st)
                    || ((ns_failed == candidate->net_st) && candidate->reconn_cnt < max_reconnect_times))
                {
                    try
                    {
                        LOG_DEBUG << " nodeid: " << candidate->node_id << "p2p net service connect peer address: " << candidate->tcp_ep;

                        candidate->last_conn_tm = time(nullptr);
                        candidate->net_st = ns_in_use;
                        candidate->reconn_cnt++;

                        //case: inverse connect to peer
                        if (exist_peer_node(candidate->node_id) || exist_peer_node(candidate->tcp_ep))
                        {
                            candidate->reconn_cnt = 0;
                            LOG_DEBUG << "tcp channel exist to: " << candidate->tcp_ep.address().to_string() << ":" << candidate->tcp_ep.port();
                            continue;
                        }

                        //case: my own node id and addr in list
                        if (CONF_MANAGER->get_node_id() == candidate->node_id)
                        {
                            candidate->net_st = ns_zombie;
                            continue;
                        }

                        //connect
                        int32_t ret = CONNECTION_MANAGER->start_connect(candidate->tcp_ep, &matrix_client_socket_channel_handler::create);
                        new_conn_cnt++;

                        if (E_SUCCESS != ret)
                        {
                            LOG_ERROR << "matrix init connector invalid peer address: " << candidate->tcp_ep;
                            candidate->net_st = ns_failed;
                        }
                    }
                    catch (const std::exception &e)
                    {
                        candidate->net_st = ns_failed;
                        LOG_ERROR << "timer connect ip catch exception. addr info: " << candidate->tcp_ep << ", " << e.what();
                    }
                }
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_timer_dyanmic_adjust_network(std::shared_ptr<matrix::core::core_timer> timer)
        {
            uint32_t client_peer_nodes_count = get_peer_nodes_count_by_socket_type(CLIENT_SOCKET);
            LOG_DEBUG << "p2p net service peer nodes map count: " << m_peer_nodes_map.size() << ", client peer nodes count: " << client_peer_nodes_count << ", peer candidates count: " << m_peer_candidates.size();

            //neighbor node is not enough
            if (client_peer_nodes_count < max_connected_cnt)
            {
                uint32_t get_count = (uint32_t)(max_connected_cnt - client_peer_nodes_count);
                std::vector<std::shared_ptr<peer_candidate>> available_candidates;
                
                //get peer candidates
                if (E_SUCCESS != get_available_peer_candidates(get_count, available_candidates))
                {
                    LOG_ERROR << "p2p net service get addr good peer candidates error";
                    return E_DEFAULT;
                }

                LOG_ERROR << "p2p net service get addr good peer candidates count: " << available_candidates.size();

                //start connect
                for (auto it : available_candidates)
                {
                    if (E_SUCCESS != start_connect(it->tcp_ep))
                    {
                        LOG_ERROR << "p2p net service dynamic adjust network and start connect error: " << it->tcp_ep.address().to_string() << " port: " << it->tcp_ep.port();
                        it->net_st = ns_failed;                        
                    }
                    else
                    {
                        LOG_ERROR << "p2p net service dynamic adjust network and start connect: " << it->tcp_ep.address().to_string() << " port: " << it->tcp_ep.port();
                        it->net_st = ns_in_use;
                    }
                }

                return E_SUCCESS;
            }
            //dynamic disconnect peer nodes
            else
            {
                //normal node with addr good status
                uint32_t available_normal_nodes_count = get_available_peer_candidates_count_by_node_type(NORMAL_NODE);
                if (available_normal_nodes_count >= MIN_NORMAL_AVAILABLE_NODE_COUNT)
                {
                    std::shared_ptr<peer_node> node = get_dynamic_disconnect_peer_node();
                    if (nullptr == node)
                    {
                        LOG_DEBUG << "p2p net service does not find dynamic disconnect peer nodes";
                        return E_DEFAULT;
                    }

                    //stop tcp socket channel
                    LOG_DEBUG << "p2p net service dynamic disconnect peer node: " << node->m_id << node->m_sid.to_string();
                    m_peer_nodes_map.erase(node->m_id);
                    CONNECTION_MANAGER->stop_channel(node->m_sid);         //whether should set ip candidates with available status if not will be erase from candidates

                    //get connect candidate
                    auto connect_candidate = get_dynamic_connect_peer_candidate();
                    if (nullptr == connect_candidate)
                    {
                        return E_DEFAULT;
                    }

                    //update net state to available after get connect candidate
                    tcp::endpoint node_ep(address_v4::from_string(node->m_peer_addr.get_ip()), (uint16_t)node->m_peer_addr.get_port());
                    auto candidate = get_peer_candidate(node_ep);
                    if (nullptr != candidate)
                    {
                        candidate->net_st = ns_available;

                        remove_peer_candidate(node_ep);
                        m_peer_candidates.push_back(candidate);
                    }

                    //create new tcp connector
                    if (E_SUCCESS != start_connect(connect_candidate->tcp_ep))
                    {
                        LOG_ERROR << "p2p net service dynamic adjust network and start connect error: " << connect_candidate->tcp_ep.address() << " port: " << connect_candidate->tcp_ep.port();
                        connect_candidate->net_st = ns_failed;
                        return E_DEFAULT;
                    }
                    else
                    {
                        LOG_DEBUG << "p2p net service dynamic adjust network and start connect: " << connect_candidate->tcp_ep.address() << " port: " << connect_candidate->tcp_ep.port();
                        connect_candidate->net_st = ns_in_use;
                        return E_SUCCESS;
                    }
                }
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_timer_peer_info_exchange(std::shared_ptr<matrix::core::core_timer> timer)
        {
            //push
            send_put_peer_nodes(nullptr);

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_timer_peer_candidate_dump(std::shared_ptr<matrix::core::core_timer> timer)
        {
            int32_t ret = save_peer_candidates();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "save peer candidates failed.";
            }

            return ret;
        }

        bool p2p_net_service::add_peer_node(std::shared_ptr<message> &msg)
        { 
            if (!msg)
            {
                LOG_ERROR << "null ptr of msg";
                return false;
            }

            string nid;
            socket_id sid = msg->header.src_sid;
            int32_t core_version, protocol_version;
            network_address peer_addr;

            if (msg->get_name() == VER_REQ)
            {
                auto req_content = std::dynamic_pointer_cast<matrix::service_core::ver_req>(msg->content);
                if (!req_content)
                {
                    LOG_ERROR << "ver_req, req_content is null.";
                    return false;
                }

                nid = req_content->body.node_id;
                core_version = req_content->body.core_version;
                protocol_version = req_content->body.protocol_version;
            }
            else if (msg->get_name() == VER_RESP)
            {
                auto rsp_content = std::dynamic_pointer_cast<matrix::service_core::ver_resp>(msg->content);
                if (!rsp_content)
                {
                    LOG_ERROR << "ver_resp, rsp_content is null.";
                    return false;
                }
                nid = rsp_content->body.node_id;
                core_version = rsp_content->body.core_version;
                protocol_version = rsp_content->body.protocol_version;
            }
            else
            {
                LOG_ERROR << "add peer node unknown msg: " << msg->get_name();
                return false;
            }
            
            LOG_DEBUG << "add peer: " << nid << sid.to_string();
            if (nid.empty())
            {
                return false;
            }

            if (CONF_MANAGER->get_node_id() == nid)
            {
                LOG_DEBUG << "node id of my own: " << nid;
                return false;
            }

            if (m_peer_nodes_map.find(nid) != m_peer_nodes_map.end())
            {
                LOG_WARNING << "duplicated node id: " << nid;
                return false;
            }
          
            tcp::endpoint ep;
            auto ptr_ch = CONNECTION_MANAGER->get_channel(sid);
            if (!ptr_ch)
            {
                LOG_ERROR << "not find in connected channels: " << sid.to_string();
                return false;
            }     
            auto ptr_tcp_ch = std::dynamic_pointer_cast<matrix::core::tcp_socket_channel>(ptr_ch);
            if (ptr_tcp_ch)
            {
                //prerequisite: channel has started
                ep = ptr_tcp_ch->get_remote_addr();
            }
            else
            {
                LOG_ERROR << nid << "not find in connected channels.";
                return false;
            }

            ptr_tcp_ch->set_remote_node_id(nid); //set remote node id of the tcp channel, and the node id will be used for efficient resp message transport.

            std::shared_ptr<peer_node> node = std::make_shared<peer_node>();
            node->m_id = nid;
            node->m_sid = sid;
            node->m_core_version = core_version;
            node->m_protocol_version = protocol_version;
            node->m_connected_time = std::time(nullptr);
            node->m_live_time = 0;
            node->m_connection_status = CONNECTED;
            node->m_peer_addr = ep;
            node->m_local_addr = ptr_tcp_ch->get_local_addr();

            if (msg->get_name() == VER_RESP)
            {
                auto candidate = get_peer_candidate(ep);
                if (candidate)
                {
                    node->m_node_type = candidate->node_type;
                }
            }

            m_peer_nodes_map[node->m_id] = node;

            LOG_DEBUG << "add a new peer_node: " << node->m_id << ", remote addr: " << ep.address().to_string() << ":" << ep.port() << sid.to_string();

            return true;
        }
        
        void p2p_net_service::remove_peer_node(const std::string &id)
        {
            m_peer_nodes_map.erase(id);
            LOG_INFO << "remove node: " << id;
        }

        bool p2p_net_service::exist_peer_node(tcp::endpoint ep)
        {
            endpoint_address addr(ep);

            //check if dest is itself
            if (addr.get_ip() == m_host_ip && addr.get_port() == m_net_listen_port)
            {
                return true;
            }

            for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); ++it)
            {
                if (it->second->m_peer_addr == addr)
                    return true;
            }

            return false;
        }

        bool p2p_net_service::exist_peer_node(std::string &nid)
        {
            if (nid.empty())
                return false;

            auto it = m_peer_nodes_map.find(nid);
            if (it != m_peer_nodes_map.end())
                return true;

            return false;
        }

        int32_t p2p_net_service::on_ver_req(std::shared_ptr<message> &msg)
        {
            auto req_content = std::dynamic_pointer_cast<matrix::service_core::ver_req>(msg->content);
            if (!req_content)
            {
                LOG_ERROR << "recv ver_req, but req_content is null.";
                return E_DEFAULT;
            }

            if (id_generator().check_base58_id(req_content->header.nonce) != true)
            {
                LOG_DEBUG << "p2p_net_service ver_req. nonce error ";
                return E_DEFAULT;
            }

            //if (!req_content->__isset.body)
            //{
            //    LOG_ERROR << "recv ver_req, but body of req_content is not set.";
            //    return E_DEFAULT;
            //}

            LOG_DEBUG << "p2p net service received ver req, node id: " << req_content->body.node_id
                << ", core_ver=" << req_content->body.core_version
                << ", protocol_ver=" << req_content->body.protocol_version;

            //add new peer node
            if(!add_peer_node(msg))
            {
                LOG_ERROR << "add node( " << req_content->body.node_id << " ) failed.";

                LOG_DEBUG << "p2p net service stop channel" << msg->header.src_sid.to_string();
                CONNECTION_MANAGER->stop_channel(msg->header.src_sid);

                return E_DEFAULT;
            }

            if (req_content->header.exten_info.count("capacity"))
            {
                LOG_DEBUG << "save remote peer's capacity " << req_content->header.exten_info["capacity"];
                CONNECTION_MANAGER->set_proto_capacity(msg->header.src_sid,
                                                       req_content->header.exten_info["capacity"]);
            }

            std::string sign_req_msg =  boost::str(boost::format("%s%s%d%d") %req_content->header.nonce %req_content->body.node_id 
                                                              %req_content->body.core_version %req_content->body.protocol_version);
            if (! ai_crypto_util::verify_sign(sign_req_msg, req_content->header.exten_info,req_content->body.node_id))
            {
                LOG_ERROR << "fake message. " << req_content->body.node_id;
                return E_DEFAULT;
            }


            std::shared_ptr<message> resp_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::ver_resp> resp_content = std::make_shared<matrix::service_core::ver_resp>();

            //header
            resp_content->header.__set_magic(CONF_MANAGER->get_net_flag());
            resp_content->header.__set_msg_name(VER_RESP);
            resp_content->header.__set_nonce(id_generator().generate_nonce());

            //capacity
            std::map<std::string, std::string> exten_info;
            exten_info["capacity"] = CONF_MANAGER->get_proto_capacity().to_string();
            

            //body
            resp_content->body.__set_node_id(CONF_MANAGER->get_node_id());
            resp_content->body.__set_core_version(CORE_VERSION);
            resp_content->body.__set_protocol_version(PROTOCO_VERSION);

            std::string sign_msg =  boost::str(boost::format("%s%s%d%d") %resp_content->header.nonce %CONF_MANAGER->get_node_id() %CORE_VERSION %PROTOCO_VERSION);
            if (E_SUCCESS != ai_crypto_util::extra_sign_info(sign_msg, exten_info))
            {
                return E_DEFAULT;
            }
            resp_content->header.__set_exten_info(exten_info);

            resp_msg->set_content(resp_content);
            resp_msg->set_name(VER_RESP);
            resp_msg->header.dst_sid = msg->header.src_sid;

            CONNECTION_MANAGER->send_message(resp_msg->header.dst_sid, resp_msg);

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_ver_resp(std::shared_ptr<message> &msg)
        {
            std::shared_ptr<matrix::service_core::ver_resp> resp_content = std::dynamic_pointer_cast<matrix::service_core::ver_resp>(msg->content);
            if (!resp_content)
            {
                LOG_ERROR << "recv ver_resp, but resp_content is null.";
                return E_DEFAULT;
            }

            if (id_generator().check_base58_id(resp_content->header.nonce) != true)
            {
                LOG_DEBUG << "p2p_net_service ver_resp. nonce error ";
                return E_DEFAULT;
            }

            std::string sign_msg =  boost::str(boost::format("%s%s%d%d") %resp_content->header.nonce %resp_content->body.node_id %resp_content->body.core_version %resp_content->body.protocol_version);
            if (! ai_crypto_util::verify_sign(sign_msg, resp_content->header.exten_info, resp_content->body.node_id))
            {
                LOG_ERROR << "fake message. " << resp_content->body.node_id;
                return E_DEFAULT;
            }

            LOG_DEBUG << "p2p net service received ver resp, node id: " << resp_content->body.node_id
                << ", core_ver=" << resp_content->body.core_version
                << ", protocol_ver=" << resp_content->body.protocol_version;

            auto ch = CONNECTION_MANAGER->get_channel(msg->header.src_sid);
            auto tcp_ch = std::dynamic_pointer_cast<tcp_socket_channel>(ch);
            if (nullptr == ch || nullptr == tcp_ch)
            {
                LOG_ERROR << "p2p net service on ver resp get channel error," << msg->header.src_sid.to_string() << "node id: " << resp_content->body.node_id;
                return E_DEFAULT;
            }

            auto candidate = get_peer_candidate(tcp_ch->get_remote_addr());
            if (nullptr != candidate)
            {
                candidate->node_id = resp_content->body.node_id;
            }
            else
            {
                LOG_ERROR << "recv a ver resp, but it it NOT found in peer candidates:" << resp_content->body.node_id << msg->header.src_sid.to_string();
            }

            //larger than max connected count, so just tag its status with available and stop channel
            uint32_t client_peer_nodes_count = get_peer_nodes_count_by_socket_type(CLIENT_SOCKET);
            if (client_peer_nodes_count >= max_connected_cnt)
            {
                LOG_DEBUG << "current client peer nodes count: " << client_peer_nodes_count
                    << " larger than max connected count,  MUST stop client tcp connection:" << msg->header.src_sid.to_string();

                if (nullptr != candidate)
                {
                    candidate->net_st = ns_available;
                }
                
                CONNECTION_MANAGER->stop_channel(msg->header.src_sid);
                return E_SUCCESS;
            }

            //add new peer node
            if(!add_peer_node(msg))
            {
                LOG_ERROR << "add node: " << resp_content->body.node_id << " failed.";

                LOG_DEBUG << "p2p net service stop channel" << msg->header.src_sid.to_string();
                CONNECTION_MANAGER->stop_channel(msg->header.src_sid);

                return E_DEFAULT;
            }
            if (candidate)
            {
                candidate->reconn_cnt = 0;
            }

            if (resp_content->header.exten_info.count("capacity"))
            {
                LOG_DEBUG << "save remote peer's capacity " << resp_content->header.exten_info["capacity"];
                CONNECTION_MANAGER->set_proto_capacity(msg->header.src_sid,
                                                       resp_content->header.exten_info["capacity"]);
            }

            tcp::endpoint local_ep = tcp_ch->get_local_addr();
            advertise_local(local_ep, msg->header.src_sid);            //advertise local self address to neighbor peer node

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_tcp_channel_error(std::shared_ptr<message> &msg)
        {
            if (!msg)
            {
                LOG_ERROR << "null ptr of msg";
                return E_NULL_POINTER;
            }

            socket_id sid = msg->header.src_sid;
            //find and update peer candidate
            std::shared_ptr<tcp_socket_channel_error_msg> err_msg = std::dynamic_pointer_cast<tcp_socket_channel_error_msg>(msg);
            if (!err_msg)
            {
                LOG_ERROR << "null ptr of err_msg: " << sid.to_string();
                return E_NULL_POINTER;
            }  
            LOG_ERROR << "p2p net service received tcp channel error msg, " << sid.to_string() << "---" << err_msg->ep.address().to_string() << ":" << err_msg->ep.port();

            auto candidate = get_peer_candidate(err_msg->ep);
            if (nullptr != candidate)
            {
                if (candidate->net_st != ns_zombie && candidate->net_st != ns_available)
                {
                    candidate->last_conn_tm = time(nullptr);
                    candidate->net_st = ns_failed;
                    
                    LOG_DEBUG << "move peer: " << candidate->tcp_ep << " to the tail of candidate list, sid: " << sid.to_string();
                    remove_peer_candidate(err_msg->ep);
                    m_peer_candidates.push_back(candidate);
                }
            }

            //rm peer_node                
            LOG_DEBUG << "sizeof m_peer_nodes_map is " << m_peer_nodes_map.size() << "; rm node: " << sid.to_string();
            for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); ++it)
            {
                if (it->second->m_sid == sid)
                {                    
                    LOG_INFO << "remove node: " << it->first << sid.to_string();
                    m_peer_nodes_map.erase(it);
                    break;
                }
            }
            LOG_DEBUG << "sizeof m_peer_nodes_map is " << m_peer_nodes_map.size();

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_client_tcp_connect_notification(std::shared_ptr<message> &msg)
        {
            auto notification_content = std::dynamic_pointer_cast<client_tcp_connect_notification>(msg);
            if (!notification_content)
            {
                return E_DEFAULT;
            }
            
            //find peer candidate
            auto candidate = get_peer_candidate(notification_content->ep);
            if (nullptr == candidate)
            {
                LOG_ERROR << "a client tcp connection established, but not in peer candidate: "
                    << notification_content->ep.address() << ":" << notification_content->ep.port();
                CONNECTION_MANAGER->release_connector(msg->header.src_sid);
                CONNECTION_MANAGER->stop_channel(msg->header.src_sid);
                return E_DEFAULT;
            }

            if (CLIENT_CONNECT_SUCCESS == notification_content->status)
            {
                //candidate->reconn_cnt = 0;//case: update when recv ver_resp

                //create ver_req message
                std::shared_ptr<message> req_msg = std::make_shared<message>();
                std::shared_ptr<matrix::service_core::ver_req> req_content = std::make_shared<matrix::service_core::ver_req>();

                //header
                req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
                req_content->header.__set_msg_name(VER_REQ);
                req_content->header.__set_nonce(id_generator().generate_nonce());

                //capacity
                std::map<std::string, std::string> exten_info;
                exten_info["capacity"] = CONF_MANAGER->get_proto_capacity().to_string();
                

                //body
                req_content->body.__set_node_id(CONF_MANAGER->get_node_id());
                req_content->body.__set_core_version(CORE_VERSION);
                req_content->body.__set_protocol_version(PROTOCO_VERSION);
                req_content->body.__set_time_stamp(std::time(nullptr));
                
                network_address addr_me;
                addr_me.__set_ip(get_host_ip());
                addr_me.__set_port(get_net_listen_port());
                req_content->body.__set_addr_me(addr_me);
                tcp::endpoint ep = std::dynamic_pointer_cast<client_tcp_connect_notification>(msg)->ep;
                network_address addr_you;
                addr_you.__set_ip(ep.address().to_string());
                addr_you.__set_port(ep.port());
                req_content->body.__set_addr_you(addr_you);
                req_content->body.__set_start_height(0);              //later

                std::string sign_msg =  boost::str(boost::format("%s%s%d%d") %req_content->header.nonce %CONF_MANAGER->get_node_id() %CORE_VERSION %PROTOCO_VERSION);
                if (E_SUCCESS != ai_crypto_util::extra_sign_info(sign_msg, exten_info))
                {
                    return E_DEFAULT;
                }
                req_content->header.__set_exten_info(exten_info);
                LOG_INFO << "send ver_req to peer, ip: " << addr_you.ip << ", port: " << addr_you.port;
                
                req_msg->set_content(req_content);
                req_msg->set_name(VER_REQ);
                req_msg->header.dst_sid = msg->header.src_sid;

                CONNECTION_MANAGER->send_message(req_msg->header.dst_sid, req_msg);
            }
            else
            {
                //update peer candidate info
                candidate->last_conn_tm = time(nullptr);        //time resume after connection finished.
                candidate->net_st = ns_failed;
            }

            CONNECTION_MANAGER->release_connector(msg->header.src_sid);
            return E_SUCCESS;
        }

#if 0
        int32_t p2p_net_service::on_cmd_get_peer_nodes_req(std::shared_ptr<message> &msg)
        {
            auto cmd_resp = std::make_shared<matrix::service_core::cmd_get_peer_nodes_resp>();
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";

            std::shared_ptr<base> content = msg->get_content();
            auto req = std::dynamic_pointer_cast<matrix::service_core::cmd_get_peer_nodes_req>(content);
            assert(nullptr != req && nullptr != content);
            COPY_MSG_HEADER(req,cmd_resp);
            if (!req || !content)
            {
                LOG_ERROR << "null ptr of cmd_get_peer_nodes_req";
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "internal error";
                TOPIC_MANAGER->publish<void>(typeid(matrix::service_core::cmd_get_peer_nodes_resp).name(), cmd_resp);

                return E_DEFAULT;
            }

            if(req->flag == matrix::service_core::flag_active)
            {
                for (auto itn = m_peer_nodes_map.begin(); itn != m_peer_nodes_map.end(); ++itn)
                {
                    matrix::service_core::cmd_peer_node_info node_info;
                    node_info.peer_node_id = itn->second->m_id;
                    node_info.live_time_stamp = itn->second->m_live_time;
                    node_info.addr.ip = itn->second->m_peer_addr.get_ip();
                    node_info.addr.port = itn->second->m_peer_addr.get_port();
                    node_info.node_type = (int8_t)itn->second->m_node_type;
                    node_info.service_list.clear();
                    node_info.service_list.push_back(std::string("ai_training"));
                    cmd_resp->peer_nodes_list.push_back(std::move(node_info));
                }
            }
            else if(req->flag == matrix::service_core::flag_global)
            {
                //case: not too many peers
                for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end(); ++it)
                {
                    matrix::service_core::cmd_peer_node_info node_info;
                    node_info.peer_node_id = (*it)->node_id;
                    node_info.live_time_stamp = 0;
                    node_info.net_st = (int8_t)(*it)->net_st;
                    node_info.addr.ip = (*it)->tcp_ep.address().to_string();
                    node_info.addr.port = (*it)->tcp_ep.port();
                    node_info.node_type = (int8_t)(*it)->node_type;
                    node_info.service_list.clear();
                    node_info.service_list.push_back(std::string("ai_training"));
                    cmd_resp->peer_nodes_list.push_back(std::move(node_info));
                }
            }

            TOPIC_MANAGER->publish<void>(typeid(matrix::service_core::cmd_get_peer_nodes_resp).name(), cmd_resp);

            return E_SUCCESS;
        }
#endif
        int32_t p2p_net_service::on_get_peer_nodes_req(std::shared_ptr<message> &msg)
        {
#if 0
            if (m_peer_nodes_map.size() == 0)
            {
                //ignore request
                return E_SUCCESS;
            }
            std::shared_ptr<matrix::service_core::get_peer_nodes_req> req = std::dynamic_pointer_cast<matrix::service_core::get_peer_nodes_req>(msg->content);
            if (!req)
            {
                LOG_ERROR << "recv get_peer_nodes_req, but req_content is null.";
                return E_DEFAULT;
            }

            if (id_generator().check_base58_id(req->header.nonce) != true)
            {
                LOG_DEBUG << "p2p_net_service on_get_peer_nodes_req. nonce error ";
                return E_DEFAULT;
            }

            const uint32_t max_peer_cnt_per_pack = 50;
            const uint32_t max_pack_cnt = 10;            
            auto it = m_peer_nodes_map.begin();
            for (uint32_t i = 0; (i <= m_peer_nodes_map.size() / max_peer_cnt_per_pack) && (i < max_pack_cnt); i++)
            {
                //common header
                std::shared_ptr<message> resp_msg = std::make_shared<message>();
                resp_msg->header.msg_name  = P2P_GET_PEER_NODES_RESP;
                resp_msg->header.msg_priority = 0;
                resp_msg->header.dst_sid = msg->header.src_sid;
                auto resp_content = std::make_shared<matrix::service_core::get_peer_nodes_resp>();
                //header
                resp_content->header.__set_magic(CONF_MANAGER->get_net_flag());
                resp_content->header.__set_msg_name(P2P_GET_PEER_NODES_RESP);
                resp_content->header.__set_nonce(id_generator().generate_nonce());

                for (uint32_t peer_cnt = 0; (it != m_peer_nodes_map.end()) && (peer_cnt < max_peer_cnt_per_pack); ++it)
                {
                    //body
                    if (it->second->m_id == CONF_MANAGER->get_node_id())
                    {
                        resp_msg->header.src_sid = it->second->m_sid;
                        continue;
                    }
                    matrix::service_core::peer_node_info info;
                    assign_peer_info(info, it->second);
                    info.service_list.push_back(std::string("ai_training"));
                    resp_content->body.peer_nodes_list.push_back(std::move(info));
                    ++peer_cnt;
                }

                resp_msg->set_content(resp_content);

                CONNECTION_MANAGER->send_message(msg->header.src_sid, resp_msg);
            }
#endif

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_get_peer_nodes_resp(std::shared_ptr<message> &msg)
        {
            //limit of candidates
            if (m_peer_candidates.size() >= MAX_PEER_CANDIDATES_CNT)
            {
                return E_SUCCESS;
            }

            std::shared_ptr<matrix::service_core::get_peer_nodes_resp> rsp = std::dynamic_pointer_cast<matrix::service_core::get_peer_nodes_resp>(msg->content);
            if (!rsp)
            {
                LOG_ERROR << "recv get_peer_nodes_resp, but req_content is null.";
                return E_DEFAULT;
            }

            if (id_generator().check_base58_id(rsp->header.nonce) != true)
            {
                LOG_DEBUG << "p2p_net_service on_get_peer_nodes_resp. nonce error ";
                return E_SUCCESS;
            }

            //if advertise local ip, peer nodes list is just one and should relay to neighbor node
            if (1 == rsp->body.peer_nodes_list.size())
            {
                const peer_node_info &node = rsp->body.peer_nodes_list[0];

                try
                {
                    tcp::endpoint ep(address_v4::from_string(node.addr.ip), (uint16_t)node.addr.port);

                    LOG_DEBUG << "p2p net service relay peer node " << node.addr.ip << ":" << (uint16_t)node.addr.port
                        << ", node_id: " << node.peer_node_id << msg->header.src_sid.to_string();

                    if (net_address::is_rfc1918(ep))
                    {
                        LOG_ERROR << "Peer Ip address is RFC1918 prive network. ip: " << ep.address().to_string();
                        return false;
                    }

                    //add to peer candidates
                    if (false == add_peer_candidate(ep, ns_available, NORMAL_NODE, node.peer_node_id))
                    {
                        LOG_DEBUG << "p2p net service add peer candidate error: " << ep;
                    }
                    else
                    {
                        LOG_DEBUG << "p2p net service add peer candidate: " << ep;
                    }
                    
                    return E_SUCCESS;
                }
                catch (boost::system::system_error e)
                {
                    LOG_ERROR << "recv a peer error: " << node.addr.ip << ", port: " << (uint16_t)node.addr.port
                        << ", node_id: " << node.peer_node_id << msg->header.src_sid.to_string()
                        << e.what();
                    return E_DEFAULT;
                }
                catch (...)
                {
                    LOG_ERROR << "recv a peer error: " << node.addr.ip << ", port: " << (uint16_t)node.addr.port
                        << ", node_id: " << node.peer_node_id << msg->header.src_sid.to_string();
                    return E_DEFAULT;
                }
            }
            
            for (auto it = rsp->body.peer_nodes_list.begin(); it != rsp->body.peer_nodes_list.end(); ++it)
            {
                try
                {
                    tcp::endpoint ep(address_v4::from_string(it->addr.ip), (uint16_t)it->addr.port);
                    LOG_DEBUG << "p2p net service received peer node: " << ", recv a peer: " << it->addr.ip << ":" << (uint16_t)it->addr.port
                        << ", node_id: " << it->peer_node_id << msg->header.src_sid.to_string();
                    
                    if (net_address::is_rfc1918(ep))
                    {
                        LOG_ERROR << "Peer Ip address is RFC1918 prive network. ip: " << ep.address().to_string();
                        continue;
                    }

                    //is in list
                    auto candidate = get_peer_candidate(ep);
                    if (nullptr == candidate)
                    {
                        //std::shared_ptr<peer_candidate> pc = std::make_shared<peer_candidate>(ep, ns_idle);
                        //pc->node_id = it->peer_node_id;
                        //m_peer_candidates.push_back(pc);
                        if (false == add_peer_candidate(ep, ns_idle, NORMAL_NODE))
                        {
                            LOG_DEBUG << "p2p net service add peer candidate error: " << ep;
                        }
                        else
                        {

                            LOG_DEBUG << "p2p net service add peer candidate: " << ep;
                        }
                    }
                    else if(candidate->node_id.empty() && !it->peer_node_id.empty())
                    {
                        candidate->node_id = it->peer_node_id;
                    }
                }
                catch (boost::system::system_error e)
                {
                    LOG_ERROR << "recv a peer but error: " << e.what();
                    continue;
                }
                catch (...)
                {
                    LOG_DEBUG << "recv a peer(" << it->addr.ip << ":" << (uint16_t)it->addr.port << ")" << ", but failed to parse.";
                    continue;
                }
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::send_get_peer_nodes()
        {
            std::shared_ptr<matrix::service_core::get_peer_nodes_req> req_content = std::make_shared<matrix::service_core::get_peer_nodes_req>();
            req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
            req_content->header.__set_msg_name(P2P_GET_PEER_NODES_REQ);
            req_content->header.__set_nonce(id_generator().generate_nonce());

            std::shared_ptr<message> req_msg = std::make_shared<message>();
            req_msg->set_name(P2P_GET_PEER_NODES_REQ);
            req_msg->set_content(req_content);
            CONNECTION_MANAGER->broadcast_message(req_msg);

            return E_SUCCESS;
        }

        int32_t p2p_net_service::send_put_peer_nodes(std::shared_ptr<peer_node> node)
        {
            //common header
            std::shared_ptr<message> resp_msg = std::make_shared<message>();
            resp_msg->header.msg_name = P2P_GET_PEER_NODES_RESP;
            resp_msg->header.msg_priority = 0;
            std::shared_ptr<matrix::service_core::get_peer_nodes_resp> resp_content = std::make_shared<matrix::service_core::get_peer_nodes_resp>();
            
            //header
            resp_content->header.__set_magic(CONF_MANAGER->get_net_flag());
            resp_content->header.__set_msg_name(P2P_GET_PEER_NODES_RESP);
            resp_content->header.__set_nonce(id_generator().generate_nonce());

            if (node)//broadcast one node
            {
                //body
                matrix::service_core::peer_node_info info;
                assign_peer_info(info, node);
                info.service_list.push_back(std::string("ai_training"));
                resp_content->body.peer_nodes_list.push_back(std::move(info));
                resp_msg->set_content(resp_content);

                CONNECTION_MANAGER->send_message(node->m_sid,resp_msg);
            }
            else// broadcast all nodes
            {
                int count = 0;
                for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); ++it)
                {
                    if (nullptr == it->second || SERVER_SOCKET == it->second->m_sid.get_type() || SEED_NODE == it->second->m_node_type)             //NAT IP is avoided to broadcast
                    {
                        continue;
                    }
                    
                    matrix::service_core::peer_node_info info;
                    assign_peer_info(info, it->second);
                    info.service_list.push_back(std::string("ai_training"));
                    resp_content->body.peer_nodes_list.push_back(std::move(info));

                    if (++count >= MAX_SEND_PEER_NODES_COUNT)
                    {
                        LOG_DEBUG << "p2p net service send peer nodes too many and break: " << m_peer_nodes_map.size();
                        break;
                    }
                }
                
                std::list<std::shared_ptr<peer_candidate> > tmp_candi_list;
                for (auto it = m_peer_candidates.begin(); (it != m_peer_candidates.end()) && (count < MAX_SEND_PEER_NODES_COUNT); )
                {
                    if (ns_available == (*it)->net_st && NORMAL_NODE == (*it)->node_type)
                    {
                        matrix::service_core::peer_node_info info;
                        
                        info.peer_node_id = (*it)->node_id;
                        info.live_time_stamp = (*it)->last_conn_tm;
                        info.addr.ip = (*it)->tcp_ep.address().to_string();
                        info.addr.port = (*it)->tcp_ep.port();

                        info.service_list.push_back(std::string("ai_training"));
                        resp_content->body.peer_nodes_list.push_back(std::move(info));
                        ++count;
                        tmp_candi_list.push_back(*it);
                        it = m_peer_candidates.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
                m_peer_candidates.insert(m_peer_candidates.end(), tmp_candi_list.begin(), tmp_candi_list.end());

                //case: make sure msg len not exceed MAX_BYTE_BUF_LEN(MAX_MSG_LEN)
                if (resp_content->body.peer_nodes_list.size() > 0)
                {
                    LOG_DEBUG << "p2p net service send peer nodes, count: " << resp_content->body.peer_nodes_list.size();

                    resp_msg->set_content(resp_content);
                    CONNECTION_MANAGER->broadcast_message(resp_msg);
                }
                else
                {
                    return E_DEFAULT;
                }
            }

            return E_SUCCESS;
        }

        bool p2p_net_service::is_peer_candidate_exist(tcp::endpoint &ep)
        {
            auto it = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end()
                , [=](std::shared_ptr<peer_candidate>& pc) -> bool { return ep == pc->tcp_ep; });
            
            return it != m_peer_candidates.end();
        }

        bool p2p_net_service::add_peer_candidate(tcp::endpoint & ep, net_state ns, peer_node_type ntype, std::string node_id)
        {     
            if (m_peer_candidates.size() >= MAX_PEER_CANDIDATES_CNT)
            {
                //limit
                return false;
            }

            auto candidate = get_peer_candidate(ep);
            if (nullptr == candidate)
            {
                auto c = std::make_shared<peer_candidate>(ep, ns, ntype, 0, time(nullptr), 0 , node_id);

                LOG_DEBUG << "node id:" << c->node_id;

                m_peer_candidates.emplace_back(c);
                return true;
            }

            return false;
        }

        bool p2p_net_service::update_peer_candidate_state(tcp::endpoint &ep, net_state ns)
        {
            auto candidate = get_peer_candidate(ep);
            if (nullptr != candidate)
            {
                candidate->net_st = ns;
                return true;
            }

            return false;
        }

        uint32_t p2p_net_service::get_peer_nodes_count_by_socket_type(socket_type type)
        {
            uint32_t count = 0;

            for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); it++)
            {
                if (nullptr != it->second && type == it->second->m_sid.get_type())
                {
                    count++;
                }
            }

            return count;
        }

        uint32_t p2p_net_service::get_maybe_available_peer_candidates_count()
        {
            uint32_t count = 0;

            for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end(); ++it)
            {
                if (ns_idle == (*it)->net_st
                    || ns_in_use == (*it)->net_st
                    || ns_available == (*it)->net_st
                    || (((*it)->net_st == ns_failed) && ((*it)->reconn_cnt < max_reconnect_times)))
                {
                    count++;
                }
            }

            return count;
        }

        int32_t p2p_net_service::get_available_peer_candidates(uint32_t count, std::vector<std::shared_ptr<peer_candidate>> &available_candidates)
        {
            if (count == 0)
            {
                return E_SUCCESS;
            }

            available_candidates.clear();
            uint32_t i = 0;

            for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end(); ++it)
            {
                if (ns_available == (*it)->net_st && i < count)
                {
                    LOG_DEBUG << "p2p net service add addr good peer candidates: " << (*it)->tcp_ep;

                    available_candidates.push_back(*it);
                    i++;
                }
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::load_peer_candidates()
        {
            m_peer_candidates.clear();

            try
            {
                ip_validator ip_vdr;
                port_validator port_vdr;

                std::shared_ptr<db_peer_candidate> db_candidate;

                //iterate peer candidates in db
                std::unique_ptr<leveldb::Iterator> it;
                it.reset(m_peers_db->NewIterator(leveldb::ReadOptions()));

                for (it->SeekToFirst(); it->Valid(); it->Next())
                {
                    db_candidate = std::make_shared<db_peer_candidate>();

                    //deserialization
                    std::shared_ptr<byte_buf> peer_candidate_buf = std::make_shared<byte_buf>();
                    peer_candidate_buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());            //may exception

                    binary_protocol proto(peer_candidate_buf.get());
                    db_candidate->read(&proto);             //may exception

                    //validate ip
                    variable_value val_ip(db_candidate->ip, false);
                    if (!ip_vdr.validate(val_ip))
                    {
                        LOG_ERROR << "p2p net service load peer candidate error: " << db_candidate->ip << " is invalid ip.";
                        continue;
                    }

                    //validate port
                    variable_value val_port(std::to_string((unsigned int)(uint16_t)db_candidate->port), false);
                    if (!port_vdr.validate(val_port))
                    {
                        LOG_ERROR << "p2p net service load peer candidate error: " << (uint16_t)db_candidate->port << " is invalid port.";
                        continue;
                    }

                    //validate node id
                    if (!db_candidate->node_id.empty() && !id_generator().check_base58_id(db_candidate->node_id))
                    {
                        LOG_ERROR << "p2p net service load peer candidate error: " << "node id: " << db_candidate->node_id << " is not Base58 code";
                        continue;
                    }

                    std::shared_ptr<peer_candidate> candidate = std::make_shared<peer_candidate>();

                    boost::asio::ip::address addr = boost::asio::ip::make_address(db_candidate->ip);
                    candidate->tcp_ep = tcp::endpoint(addr, (uint16_t)db_candidate->port);

                    candidate->net_st =ns_idle;
                    candidate->reconn_cnt = 0;
                    candidate->last_conn_tm = db_candidate->last_conn_tm;
                    candidate->score = db_candidate->score;
                    candidate->node_id = db_candidate->node_id;
                    candidate->node_type = (peer_node_type)db_candidate->node_type;

                    m_peer_candidates.push_back(candidate);
                }
            }
            catch (...)
            {
                LOG_ERROR << "p2p net service load peer candidates from db exception";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::save_peer_candidates()
        {
            LOG_DEBUG << "save peer candidates: " << m_peer_candidates.size();

            if (m_peer_candidates.empty())
            {
                return E_SUCCESS;
            }

            //clear db
            clear_peer_candidates_db();

            try
            {
                leveldb::WriteBatch batch;

                for (auto it : m_peer_candidates)
                {
                    std::shared_ptr<db_peer_candidate> db_candidate = std::make_shared<db_peer_candidate>();

                    db_candidate->__set_ip(it->tcp_ep.address().to_string());
                    db_candidate->__set_port(it->tcp_ep.port());
                    db_candidate->__set_net_state(it->net_st);
                    db_candidate->__set_reconn_cnt(it->reconn_cnt);
                    db_candidate->__set_last_conn_tm(it->last_conn_tm);
                    db_candidate->__set_score(it->score);
                    db_candidate->__set_node_id(it->node_id);
                    db_candidate->__set_node_type(it->node_type);

                    //serialization
                    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
                    binary_protocol proto(out_buf.get());
                    db_candidate->write(&proto);

                    leveldb::Slice slice(out_buf->get_read_ptr(), out_buf->get_valid_read_len());
                    batch.Put(db_candidate->ip, slice);
                }

                //flush to db
                leveldb::WriteOptions write_options;
                //write_options.sync = true;                    //no need sync

                leveldb::Status status = m_peers_db->Write(write_options, &batch);
                if (!status.ok())
                {
                    LOG_ERROR << "p2p net service save peer candidates error: " << status.ToString();
                    return E_DEFAULT;
                }

                return E_SUCCESS;
            }
            catch (...)
            {
                LOG_ERROR << "p2p net service save peer candidates error";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::clear_peer_candidates_db()
        {
            try
            {
                //iterate task in db
                std::unique_ptr<leveldb::Iterator> it;
                it.reset(m_peers_db->NewIterator(leveldb::ReadOptions()));

                for (it->SeekToFirst(); it->Valid(); it->Next())
                {
                    m_peers_db->Delete(leveldb::WriteOptions(), it->key());
                }
            }
            catch (...)
            {
                LOG_ERROR << "p2p net service clear peer candidates db exception";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::add_dns_seeds()
        {
            try
            {
                if (m_dns_seeds.empty())
                {
                    m_dns_seeds.insert(m_dns_seeds.begin(), CONF_MANAGER->get_dns_seeds().begin(), CONF_MANAGER->get_dns_seeds().end());
                }

                //get dns seeds
                const char *dns_seed = m_dns_seeds.front();
                m_dns_seeds.pop_front();

                if (nullptr == dns_seed)
                {
                    LOG_ERROR << "p2p net service resolve dns nullptr";
                    return E_DEFAULT;
                }

                io_service ios;
                ip::tcp::resolver rslv(ios);
                ip::tcp::resolver::query qry(dns_seed, boost::lexical_cast<string>(80));
                ip::tcp::resolver::iterator it = rslv.resolve(qry);
                ip::tcp::resolver::iterator end;

                for (; it != end; it++)
                {
                    LOG_DEBUG << "p2p net service add dns candidate: " << dns_seed << ", ip: " << it->endpoint().address().to_string();

                    tcp::endpoint ep(it->endpoint().address(), CONF_MANAGER->get_net_default_port());
                    add_peer_candidate(ep, ns_idle, SEED_NODE);
                }
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "p2p net service resolve dns error: " << diagnostic_information(e);
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::add_hard_code_seeds()
        {
            //get hard code seeds
            for (auto it = m_hard_code_seeds.begin(); it != m_hard_code_seeds.end(); it++)
            {
                LOG_DEBUG << "p2p net service add hard code candidate, ip: " << it->seed << ", port: " << it->port;

                tcp::endpoint ep(ip::address::from_string(it->seed), it->port);
                add_peer_candidate(ep, ns_idle, SEED_NODE);
            }

            return E_SUCCESS;
        }

        uint32_t p2p_net_service::start_connect(const tcp::endpoint tcp_ep)
        {
            try
            {
                LOG_DEBUG << "matrix connect peer address; ip: " << tcp_ep.address() << " port: " << tcp_ep.port();

                if (exist_peer_node(tcp_ep))
                {
                    LOG_DEBUG << "tcp channel exist to: " << tcp_ep.address().to_string() << ":" << tcp_ep.port();
                    return E_DEFAULT;
                }

                //start connect
                if (E_SUCCESS != CONNECTION_MANAGER->start_connect(tcp_ep, &matrix_client_socket_channel_handler::create))
                {
                    LOG_ERROR << "matrix init connector invalid peer address, ip: " << tcp_ep.address() << " port: " << tcp_ep.port();
                    return E_DEFAULT;
                }
            }
            catch (const std::exception &e)
            {                
                LOG_ERROR << "timer connect ip catch exception. addr info: " << tcp_ep.address() << " port: " << tcp_ep.port() << ", " << e.what();
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        uint32_t p2p_net_service::get_available_peer_candidates_count_by_node_type(peer_node_type node_type)
        {
            uint32_t count = 0;

            for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end(); ++it)
            {
                if (ns_available == (*it)->net_st && node_type == (*it)->node_type)
                {
                    count++;
                }
            }

            return count;
        }

        std::shared_ptr<peer_node> p2p_net_service::get_dynamic_disconnect_peer_node()
        {
            if (m_peer_nodes_map.empty())
            {
                return nullptr;
            }

            static uint32_t DISCONNECT_INTERVAL = 0;

            std::vector<std::shared_ptr<peer_node>> client_connect_peer_nodes;
            for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); it++)
            {
                //seed node is disconnected prior
                if (SEED_NODE == it->second->m_node_type && CLIENT_SOCKET == it->second->m_sid.get_type())
                {
                    LOG_DEBUG << "p2p net service get disconnect seed peer node: " << it->second->m_id << it->second->m_sid.to_string();
                    return it->second;
                }

                //add to client nodes for later use
                if (CLIENT_SOCKET == it->second->m_sid.get_type())
                {
                    client_connect_peer_nodes.push_back(it->second);
                }
            }

            if (client_connect_peer_nodes.empty())
            {
                return nullptr;
            }

            //disconnect one normal node per five minutes
            if (++DISCONNECT_INTERVAL % DISCONNECT_NODE_PER_MINUTES)
            {
                LOG_DEBUG << "it is not time to disconnect normal node and have a rest: " << DISCONNECT_INTERVAL;
                return nullptr;
            }

            //random choose client connect peer nodes to disconnect
            uint32_t rand_num = get_rand32() % client_connect_peer_nodes.size();
            std::shared_ptr<peer_node> disconnect_node = client_connect_peer_nodes[rand_num];
            LOG_DEBUG << "p2p net service get disconnect normal peer node: " << disconnect_node->m_id << disconnect_node->m_sid.to_string();

            return disconnect_node;
        }

        std::shared_ptr<peer_candidate> p2p_net_service::get_dynamic_connect_peer_candidate()
        {
            std::vector<std::shared_ptr<peer_candidate>> seed_node_candidates;
            std::vector<std::shared_ptr<peer_candidate>> normal_node_candidates;

            for (auto it : m_peer_candidates)
            {
                if (nullptr != get_peer_node(it->node_id))
                {
                    continue;
                }

                if (ns_available == it->net_st && SEED_NODE == it->node_type)
                {
                    seed_node_candidates.push_back(it);
                }

                if (ns_available == it->net_st && NORMAL_NODE == it->node_type)
                {
                    normal_node_candidates.push_back(it);
                }
            }

            LOG_DEBUG << "normal node candidates count: " << normal_node_candidates.size() << "seed node candidates count: " << seed_node_candidates.size();

            //normal good candidate prior
            if (normal_node_candidates.size() > 0)
            {
                uint32_t rand_num = get_rand32() % normal_node_candidates.size();
                std::shared_ptr<peer_candidate> connect_candidate = normal_node_candidates[rand_num];

                LOG_DEBUG << "normal node candidate: " << connect_candidate->tcp_ep.address().to_string() << ", port: " << connect_candidate->tcp_ep.port();
                return connect_candidate;
            }

            //seed good candidate next
            if (seed_node_candidates.size() > 0)
            {
                uint32_t rand_num = get_rand32() % seed_node_candidates.size();
                std::shared_ptr<peer_candidate> connect_candidate = seed_node_candidates[rand_num];

                LOG_DEBUG << "seed node candidate: " << connect_candidate->tcp_ep.address().to_string() << ", port: " << connect_candidate->tcp_ep.port();
                return connect_candidate;
            }

            LOG_ERROR << "p2p net service get no dynamic connect candidate";
            return nullptr;
        }

        void p2p_net_service::advertise_local(tcp::endpoint tcp_ep, socket_id sid)
        {
            if (net_address::is_rfc1918(tcp_ep))
            {                
                LOG_DEBUG << "ip address is RFC1918 private network ip and will not advertise local: " << tcp_ep.address().to_string();
                return;
            }

            std::shared_ptr<peer_node> node = std::make_shared<peer_node>();

            node->m_id = CONF_MANAGER->get_node_id();
            node->m_live_time = time(nullptr);
            node->m_peer_addr = tcp::endpoint(tcp_ep.address(), m_net_listen_port);
            node->m_sid = sid;

            LOG_DEBUG << "p2p net service advertise local self address: " << tcp_ep.address().to_string() << ", port: " << m_net_listen_port;
            send_put_peer_nodes(node);
        }


        std::shared_ptr<peer_candidate> p2p_net_service::get_peer_candidate(const tcp::endpoint &ep)
        {
            auto it = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end()
                , [=](std::shared_ptr<peer_candidate> & candidate) -> bool { return ep == candidate->tcp_ep; });

            return (it != m_peer_candidates.end()) ? *it : nullptr;
        }

        void p2p_net_service::remove_peer_candidate(const tcp::endpoint &ep)
        {
            auto it = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end()
                , [=](std::shared_ptr<peer_candidate> & candidate) -> bool { return ep == candidate->tcp_ep; });

            if (it != m_peer_candidates.end())
            {
                m_peer_candidates.erase(it);
                LOG_DEBUG << "p2p net service remove peer candidate: " << ep;
            }
        }

    }
}
