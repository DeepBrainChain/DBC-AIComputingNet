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
#include "handler_create_functor.h"
#include "channel.h"
#include "ip_validator.h"
#include "port_validator.h"
#include "api_call.h"
#include "id_generator.h"
#include "version.h"
#include "tcp_socket_channel.h"
#include "timer_def.h"
#include "peer_seeds.h"

using namespace std;
using namespace boost::asio::ip;
using namespace matrix::core;

const uint32_t max_reconnect_times = 2;
const uint32_t max_connected_cnt = 200;
const uint32_t max_connect_per_check = 10;

namespace matrix
{
    namespace service_core
    {
        p2p_net_service::p2p_net_service()
            : m_timer_id_one_minute(INVALID_TIMER_ID)
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

        int32_t p2p_net_service::init_acceptor()
        {
            //init ip and port
            if (E_SUCCESS != init_conf())
            {
                LOG_ERROR << "p2p_net_service init acceptor error and exit";
                return E_DEFAULT;
            }

            //ipv4 or ipv6
            tcp::endpoint ep(ip::address::from_string(m_host_ip), m_net_listen_port);

            int32_t ret = E_SUCCESS;

            //net
            LOG_DEBUG << "p2p net service init net, ip: " << m_host_ip << " port: " << m_net_listen_port;
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
                    cout <<"\"" << addr << "\"" << " is an invalid peer address" << endl;
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
                    cout << "\"" << addr << "\"" << " is an invalid peer address" << endl;
                    continue;
                }

                //validate port
                if (str_port.empty())
                {
                    LOG_ERROR << "p2p_net_service init_connect invalid port: " << str_port;
                    cout << "\"" << addr << "\"" << " is an invalid peer address" << endl;
                    continue;
                }                

                val.value() = str_port;
                if (false == port_vdr.validate(val))
                {
                    LOG_ERROR << "p2p_net_service init_connect invalid port: " << str_port;
                    cout << "\"" << addr << "\"" << " is an invalid peer address" << endl;
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
                    cout << "\"" << addr << "\"" << " is an invalid peer address" << endl;
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
                        add_peer_candidate(ep, ns_in_use);
                    }
                }
                catch (const std::exception &e)
                {
                    LOG_ERROR << "p2p_net_service init_connect abnormal. addr info: " << ip <<":"<<str_port << ", " << e.what();
                    continue;
                }

                count++;
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::service_init(bpo::variables_map &options)
        {
            int32_t ret = E_SUCCESS;

            //load peer candidates
            ret = load_peer_candidates(m_peer_candidates);
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
            m_timer_invokers[TIMER_NAME_P2P_ONE_MINUTE] = std::bind(&p2p_net_service::on_timer_one_minute, this, std::placeholders::_1);
            m_timer_id_one_minute = add_timer(TIMER_NAME_P2P_ONE_MINUTE, TIMER_INTERV_MIN_P2P_ONE_MINUTE *60 * 1000);//60s*1000
            assert(m_timer_id_one_minute != INVALID_TIMER_ID);
        }

        int32_t p2p_net_service::service_exit()
        {
            if (m_timer_id_one_minute != INVALID_TIMER_ID)
            {
                remove_timer(m_timer_id_one_minute);
                m_timer_id_one_minute = INVALID_TIMER_ID;
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_timer_one_minute(std::shared_ptr<matrix::core::core_timer> timer)
        {
            assert(timer->get_timer_id() == m_timer_id_one_minute);

            static uint32_t minutes = 0;
            ++minutes;
            
            on_timer_check_peer_candidate();

            if (minutes % TIMER_INTERV_MIN_P2P_PEER_INFO_EXCHANGE == 0)
            {
                on_timer_peer_info_exchange();
            }

            if (minutes % TIMER_INTERV_MIN_P2P_PEER_CANDIDATE_DUMP == 0)
            {
                on_timer_peer_candidate_dump();
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_timer_peer_info_exchange()
        {
            static uint32_t even_num = 0;
            ++even_num;
            //pull
            send_get_peer_nodes();

            if (even_num % 2 == 0)
            {
                even_num = 0;
                //push
                send_put_peer_nodes(nullptr);
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_timer_check_peer_candidate()
        {
            //check up-bound & remove candidate that try too much
            uint32_t in_use_peer_cnt = 0;
            for (std::list<peer_candidate>::iterator it = m_peer_candidates.begin(); it != m_peer_candidates.end(); )
            {
                if (ns_in_use == it->net_st)
                {
                    in_use_peer_cnt++;
                }
                else if ((ns_failed == it->net_st) && (it->reconn_cnt > max_reconnect_times))
                {
                    m_peer_candidates.erase(it++);
                    continue;
                }
                ++it;
            }

            if (in_use_peer_cnt >= max_connected_cnt)
            {
                return E_SUCCESS;
            }

            //use hard code peer seeds 
            //if no neighbor peer nodes and peer candidates
            if (0 == m_peer_nodes_map.size() && !has_available_peer_candidates())
            {
                if (m_dns_seeds.size() > 0)
                {
                    try
                    {
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

                        for ( ; it != end; it++)
                        {
                            LOG_DEBUG << "p2p net service resolve dns: " << dns_seed << ", ip: "<< it->endpoint().address().to_string();

                            tcp::endpoint ep(it->endpoint().address(), CONF_MANAGER->get_net_default_port()); 
                            add_peer_candidate(ep, ns_idle);
                        }
                    }
                    catch (const boost::exception & e)
                    {
                        LOG_ERROR << "p2p net service resolve dns error: " << diagnostic_information(e);
                    }
                }
                //case: maybe dns resolver produce nothing
                if (!has_available_peer_candidates())//still no available candidate
                {
                    //get hard code seeds
                    for (auto it = m_hard_code_seeds.begin(); it != m_hard_code_seeds.end(); it++)
                    {
                        LOG_DEBUG << "p2p net service add candidate, ip: " << it->seed << ", port: " << it->port;

                        tcp::endpoint ep(ip::address::from_string(it->seed), it->port);
                        add_peer_candidate(ep, ns_idle);
                    }
                }
            }

            //increase connections
            uint32_t new_conn_cnt = 0;
            for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end(); ++it)
            {
                if ((ns_idle == it->net_st) 
                    || ((ns_failed == it->net_st) 
                        && (time(nullptr) > TIMER_INTERV_MIN_P2P_CONNECT_NEW_PEER * it->reconn_cnt + it->last_conn_tm)
                        && it->reconn_cnt <= max_reconnect_times)
                    )
                {
                    //case: inverse connect to peer
                    if (!it->node_id.empty())
                    {
                        if (m_peer_nodes_map.find(it->node_id) != m_peer_nodes_map.end())
                        {
                            //it->net_st = ns_in_use;//do not change state
                            continue;
                        }
                    }
                    
                    //connect
                    it->last_conn_tm = time(nullptr);
                    it->net_st = ns_in_use;
                    it->reconn_cnt++;

                    try
                    {
                        LOG_DEBUG << "matrix connect peer address; ip: " << it->tcp_ep.address() << " port: " << it->tcp_ep.port();
                        if (exist_peer_node(it->tcp_ep))
                        {
                            LOG_DEBUG << "tcp channel exist to: " << it->tcp_ep.address().to_string() << ":" << it->tcp_ep.port();
                            continue;
                        }
                        int32_t ret = CONNECTION_MANAGER->start_connect(it->tcp_ep, &matrix_client_socket_channel_handler::create);
                        new_conn_cnt++;

                        if (E_SUCCESS != ret)
                        {
                            LOG_ERROR << "matrix init connector invalid peer address, ip: " << it->tcp_ep.address() << " port: " << it->tcp_ep.port();
                            it->net_st = ns_failed;
                            continue;
                        }
                    }
                    catch (const std::exception &e)
                    {
                        it->net_st = ns_failed;
                        LOG_ERROR << "timer connect ip catch exception. addr info: " << it->tcp_ep.address() << ":" << it->tcp_ep.port() << ", " << e.what();
                        continue;
                    }
                }     

                if (new_conn_cnt > max_connect_per_check)
                {
                    break;//not too many conn at a time
                }

                if (new_conn_cnt + in_use_peer_cnt >= max_connected_cnt)
                {
                    break;//up to high bound 
                }
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_timer_peer_candidate_dump()
        {
            int32_t ret = save_peer_candidates(m_peer_candidates);
            if (E_SUCCESS != ret)
            {
                LOG_WARNING << "save peer candidates failed.";
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
            else
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
            
            LOG_DEBUG << "add peer(" << nid << "), sid=" << sid.to_string();
            if (nid.empty())
                return false;

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

            std::shared_ptr<peer_node> node = std::make_shared<peer_node>();
            node->m_id = nid;
            node->m_sid = sid;
            node->m_core_version = core_version;
            node->m_protocol_version = protocol_version;
            node->m_connected_time = std::time(nullptr);
            node->m_live_time = 0;
            node->m_connection_status = connected;
            if (msg->get_name() == VER_RESP)
            {
                node->m_peer_addr = ep;
            }
            else
            {
                //temp: supposed addr
                node->m_peer_addr = endpoint_address(ep.address().to_string(), std::atoi(CONF_MANAGER->get_net_listen_port().c_str()));
            }
            node->m_local_addr = ptr_tcp_ch->get_local_addr();

            m_peer_nodes_map[node->m_id] = node;

            LOG_DEBUG << "add a new peer_node(" << node->m_id << "), remote addr: " << ep.address().to_string() << ":" << ep.port() << "sid=" << sid.to_string();

            return true;
        }
        
        void p2p_net_service::remove_peer_node(const std::string &id)
        {
            m_peer_nodes_map.erase(id);
            LOG_INFO << "remove node(" << id << ")";
        }

        bool p2p_net_service::exist_peer_node(tcp::endpoint ep)
        {
            endpoint_address addr(ep);

            //check if dest is itself
            if (addr.get_ip() == m_host_ip && addr.get_port() == m_net_listen_port)
                return true;

            for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); ++it)
            {
                if (it->second->m_peer_addr == addr)
                    return true;
            }

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

            std::vector<unsigned char> vchRet;
            if (DecodeBase58Check(SanitizeString(req_content->header.nonce), vchRet) != true)
            {
                LOG_DEBUG << "p2p_net_service ver_req. nonce error ";
                return E_DEFAULT;
            }

            //filter node which connects to the same node(maybe another process with same nodeid)   
            auto ch = CONNECTION_MANAGER->get_channel(msg->header.src_sid);
            if (ch)
            {
                auto tcp_ch = std::dynamic_pointer_cast<tcp_socket_channel>(ch);
                if (tcp_ch)
                {
                    std::list<peer_candidate>::iterator it = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end()
                        , [=](peer_candidate& pc) -> bool { return tcp_ch->get_remote_addr() == pc.tcp_ep; });
                    if (it != m_peer_candidates.end())
                    {    
                        if (req_content->body.node_id == CONF_MANAGER->get_node_id())
                        {
                            it->last_conn_tm = time(nullptr);
                            it->net_st = ns_zombie;

                            //stop channel
                            LOG_DEBUG << "same node_id, p2p net service stop channel: " << msg->header.src_sid.to_string();
                            CONNECTION_MANAGER->stop_channel(msg->header.src_sid);

                            return E_SUCCESS;
                        }
                        else
                        {
                            it->node_id = req_content->body.node_id;
                        }
                    }
                    else
                    {
                        LOG_INFO << "recv a new peer(" << req_content->body.node_id << ")";
                    }
                }                
            }
            else
            {
                LOG_ERROR << "p2p net service on ver req get channel error," << msg->header.src_sid.to_string() << "node id: " << req_content->body.node_id;
                return E_DEFAULT;
            }

            LOG_DEBUG << "p2p net service received ver req, node id: " << req_content->body.node_id;

            //add new peer node
            if(!add_peer_node(msg))
            {
                LOG_ERROR << "add node( " << req_content->body.node_id << " ) failed.";

                LOG_DEBUG << "p2p net service stop channel" << msg->header.src_sid.to_string();
                CONNECTION_MANAGER->stop_channel(msg->header.src_sid);

                return E_DEFAULT;
            }

            std::shared_ptr<message> resp_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::ver_resp> resp_content = std::make_shared<matrix::service_core::ver_resp>();

            //header
            resp_content->header.__set_magic(CONF_MANAGER->get_net_flag());
            resp_content->header.__set_msg_name(VER_RESP);
            resp_content->header.__set_nonce(id_generator().generate_nonce());

            //body
            resp_content->body.__set_node_id(CONF_MANAGER->get_node_id());
            resp_content->body.__set_core_version(CORE_VERSION);
            resp_content->body.__set_protocol_version(PROTOCO_VERSION);

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

            std::vector<unsigned char> vchRet;
            if (DecodeBase58Check(SanitizeString(resp_content->header.nonce), vchRet) != true)
            {
                LOG_DEBUG << "p2p_net_service ver_resp. nonce error ";
                return E_DEFAULT;
            }
            LOG_DEBUG << "p2p net service received ver resp, node id: " << resp_content->body.node_id;

            auto ch = CONNECTION_MANAGER->get_channel(msg->header.src_sid);
            if (ch)
            {
                auto tcp_ch = std::dynamic_pointer_cast<tcp_socket_channel>(ch);
                if (tcp_ch)
                {
                    auto it = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end()
                        , [=](peer_candidate& pc) -> bool { return tcp_ch->get_remote_addr() == pc.tcp_ep; });
                    if (it != m_peer_candidates.end())
                    {
                        it->node_id = resp_content->body.node_id;
                    }
                    else
                    {
                        LOG_INFO << "recv a ver resp, but it's a new peer(" << resp_content->body.node_id << ")" 
                            << "sid: " << msg->header.src_sid.to_string();
                    }
                }
            }
            else
            {
                LOG_ERROR << "p2p net service on ver resp get channel error," << msg->header.src_sid.to_string() << "node id: " << resp_content->body.node_id;
                return E_DEFAULT;
            }

            //add new peer node
            if(!add_peer_node(msg))
            {
                LOG_ERROR << "add node( " << resp_content->body.node_id << " ) failed.";

                LOG_DEBUG << "p2p net service stop channel" << msg->header.src_sid.to_string();
                CONNECTION_MANAGER->stop_channel(msg->header.src_sid);

                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_tcp_channel_error(std::shared_ptr<message> &msg)
        {
            if (!msg)
            {
                LOG_ERROR << "null ptr of msg;";
                return E_NULL_POINTER;
            }

            socket_id  sid = msg->header.src_sid;
            //find and update peer candidate
            std::shared_ptr<tcp_socket_channel_error_msg> err_msg = std::dynamic_pointer_cast<tcp_socket_channel_error_msg>(msg);
            if (!err_msg)
            {
                LOG_ERROR << "null ptr of err_msg: " << sid.to_string();
                return E_NULL_POINTER;
            }  
            LOG_ERROR << "p2p net service received tcp channel error msg, " << sid.to_string() << "---" << err_msg->ep.address().to_string() << ":" << err_msg->ep.port();

            std::list<peer_candidate>::iterator it = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end()
                , [=](peer_candidate& pc) -> bool { return err_msg->ep == pc.tcp_ep; });
            if (it != m_peer_candidates.end())
            {
                if (it->net_st != ns_zombie)
                {
                    it->last_conn_tm = time(nullptr);
                    it->net_st = ns_failed;
                    //move it to the tail
                    auto pc = *it;
                    LOG_DEBUG << "move peer(" << pc.tcp_ep << ") to the tail of candidate list, sid: " << sid.to_string();
                    m_peer_candidates.erase(it);
                    m_peer_candidates.push_back(std::move(pc));
                }
            }
            else
            {
                LOG_ERROR << "a peer_node network error occurs, but not in ip candidates: " << err_msg->ep.address() << ":" << err_msg->ep.port();
            }

            //rm peer_node                
            LOG_DEBUG << "sizeof m_peer_nodes_map is " << m_peer_nodes_map.size() << "; rm node: " << sid.to_string();
            for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); ++it)
            {
                if (it->second->m_sid == sid)
                {                    
                    LOG_INFO << "remove node(" << it->first << "), sid: " << sid.to_string();
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
            std::list<peer_candidate>::iterator it = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end()
                , [=](peer_candidate& pc) -> bool { return notification_content->ep == pc.tcp_ep; });

            if (CLIENT_CONNECT_SUCCESS == notification_content->status)
            {
                //update peer candidate info
                if (it != m_peer_candidates.end())
                {
                    it->reconn_cnt = 0;
                }
                else
                {
                    LOG_WARNING << "a client tcp connection established, but not in peer candidate: " << notification_content->ep.address() << ":" << notification_content->ep.port();
                }

                //create ver_req message
                std::shared_ptr<message> req_msg = std::make_shared<message>();
                std::shared_ptr<matrix::service_core::ver_req> req_content = std::make_shared<matrix::service_core::ver_req>();

                //header
                //req_content->header.length = 0;
                req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
                req_content->header.__set_msg_name(VER_REQ);
                req_content->header.__set_nonce(id_generator().generate_nonce());

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

                LOG_INFO << "send ver_req to peer(" << addr_you.ip << ":" << addr_you.port << ")";
                
                req_msg->set_content(req_content);
                req_msg->set_name(VER_REQ);
                req_msg->header.dst_sid = msg->header.src_sid;

                CONNECTION_MANAGER->send_message(req_msg->header.dst_sid, req_msg);

                CONNECTION_MANAGER->release_connector(msg->header.src_sid);

                return E_SUCCESS;
            }
            else
            {
                //update peer candidate info
                if (it != m_peer_candidates.end())
                {
                    it->last_conn_tm = time(nullptr);//time resume after connection finished.
                    it->net_st = ns_failed;
                }
                else
                {
                    LOG_WARNING << "a client tcp connection failed, but not in peer candidate: " << notification_content->ep.address() << ":" << notification_content->ep.port();
                }

                //cancel connect and connect next    
                CONNECTION_MANAGER->release_connector(msg->header.src_sid);

                return E_SUCCESS;
            }
        }

        int32_t p2p_net_service::on_cmd_get_peer_nodes_req(std::shared_ptr<message> &msg)
        {
            auto cmd_resp = std::make_shared<matrix::service_core::cmd_get_peer_nodes_resp>();
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";

            std::shared_ptr<base> content = msg->get_content();
            auto req = std::dynamic_pointer_cast<matrix::service_core::cmd_get_peer_nodes_req>(content);
            assert(nullptr != req && nullptr != content);
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
                    node_info.peer_node_id = it->node_id;
                    node_info.live_time_stamp = 0;
                    node_info.net_st = (int8_t)it->net_st;
                    node_info.addr.ip = it->tcp_ep.address().to_string();
                    node_info.addr.port = it->tcp_ep.port();
                    node_info.service_list.clear();
                    node_info.service_list.push_back(std::string("ai_training"));
                    cmd_resp->peer_nodes_list.push_back(std::move(node_info));
                }
            }

            TOPIC_MANAGER->publish<void>(typeid(matrix::service_core::cmd_get_peer_nodes_resp).name(), cmd_resp);

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_get_peer_nodes_req(std::shared_ptr<message> &msg)
        {
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
            std::vector<unsigned char> vchRet;
            if (DecodeBase58Check(SanitizeString(req->header.nonce), vchRet) != true)
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

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_get_peer_nodes_resp(std::shared_ptr<message> &msg)
        {
            std::shared_ptr<matrix::service_core::get_peer_nodes_resp> rsp = std::dynamic_pointer_cast<matrix::service_core::get_peer_nodes_resp>(msg->content);
            if (!rsp)
            {
                LOG_ERROR << "recv get_peer_nodes_resp, but req_content is null.";
                return E_DEFAULT;
            }

            std::vector<unsigned char> vchRet;
            if (DecodeBase58Check(SanitizeString(rsp->header.nonce), vchRet) != true)
            {
                LOG_DEBUG << "p2p_net_service on_get_peer_nodes_resp. nonce error ";
                return E_SUCCESS;
            }
            for (auto it = rsp->body.peer_nodes_list.begin(); it != rsp->body.peer_nodes_list.end(); ++it)
            {
                try
                {
                    tcp::endpoint ep(address_v4::from_string(it->addr.ip), (uint16_t)it->addr.port);
                    LOG_DEBUG << "sid: " << msg->header.src_sid.to_string() << ", recv a peer(" << it->addr.ip << ":" << it->addr.port << "), node_id: " << it->peer_node_id;
                    //is in list
                    std::list<peer_candidate>::iterator it_pc = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end()
                        , [=](peer_candidate& pc) -> bool { return ep == pc.tcp_ep; });
                    if (it_pc == m_peer_candidates.end())
                    {
                        peer_candidate pc(ep, ns_idle);
                        pc.node_id = it->peer_node_id;
                        m_peer_candidates.push_back(std::move(pc));
                    }
                    else if(it_pc->node_id.empty() && !it->peer_node_id.empty())
                    {
                        it_pc->node_id = it->peer_node_id;
                    }
                }
                catch (boost::system::system_error e)
                {
                    LOG_ERROR << "recv a peer but error: " << e.what();
                    continue;
                }
                catch (...)
                {
                    LOG_DEBUG << "recv a peer(" << it->addr.ip << ":" << it->addr.port << ")" << ", but failed to parse.";
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

                CONNECTION_MANAGER->broadcast_message(resp_msg, node->m_sid);
            }
            else// broadcast all nodes
            {
                for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); ++it)
                {
                    if (it->second->m_id == CONF_MANAGER->get_node_id())
                    {
                        //assert(0); //should never occur
                        LOG_ERROR << "peer node(" << it->second->m_id << ") is myself.";
                        continue;
                    }     
                    matrix::service_core::peer_node_info info;
                    assign_peer_info(info, it->second);
                    info.service_list.push_back(std::string("ai_training"));
                    resp_content->body.peer_nodes_list.push_back(std::move(info));
                }

                //case: make sure msg len not exceed MAX_BYTE_BUF_LEN(MAX_MSG_LEN)
                if (resp_content->body.peer_nodes_list.size() > 0)
                {
                    resp_msg->set_content(resp_content);
                    CONNECTION_MANAGER->broadcast_message(resp_msg);//filer ??
                }
                else
                {
                    return E_DEFAULT;
                }
            }

            return E_SUCCESS;
        }

        bool p2p_net_service::has_available_peer_candidates()
        {
            for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end(); ++it)
            {
                if (it->net_st <= ns_in_use || ((it->net_st == ns_failed) && (it->reconn_cnt < max_reconnect_times)))
                    return true;
            }
            return false;
        }

        bool p2p_net_service::is_peer_candidate_exist(tcp::endpoint &ep)
        {
            std::list<peer_candidate>::iterator it = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end()
                , [=](peer_candidate& pc) -> bool { return ep == pc.tcp_ep; });
            
            return it != m_peer_candidates.end();
        }

        bool p2p_net_service::add_peer_candidate(tcp::endpoint &ep, net_state ns)
        {            
            std::list<peer_candidate>::iterator it = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end()
                , [=](peer_candidate& pc) -> bool { return ep == pc.tcp_ep; });
            if (it == m_peer_candidates.end())
            {
                m_peer_candidates.emplace_back(ep, ns);
                return true;
            }

            return false;
        }

        bool p2p_net_service::update_peer_candidate_state(tcp::endpoint &ep, net_state ns)
        {
            std::list<peer_candidate>::iterator it = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end()
                , [=](peer_candidate& pc) -> bool { return ep == pc.tcp_ep; });
            if (it != m_peer_candidates.end())
            {
                it->net_st = ns;
                return true;
            }

            return false;
        }
    }
}