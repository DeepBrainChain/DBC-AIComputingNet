/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name          p2p_net_service.cpp
* description        p2p net service
* date                  : 2018.01.28
* author              Bruce Feng
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
#include "api_call_handler.h"
#include "id_generator.h"
#include "version.h"
#include "tcp_socket_channel.h"
#include "timer_def.h"

using namespace std;
using namespace boost::asio::ip;
using namespace matrix::core;

const uint32_t max_reconnect_times = 100;
const uint32_t max_connected_cnt = 200;

namespace matrix
{
    namespace service_core
    {
        p2p_net_service::p2p_net_service()
            : m_timer_id_one_minute(INVALID_TIMER_ID)
        {

        }

        int32_t p2p_net_service::init(bpo::variables_map &options)
        {
            uint32_t ret = service_module::init(options);

            return ret;
        }

        void p2p_net_service::get_all_peer_nodes(peer_list_type &nodes)
        {
            read_lock_guard<rw_lock> lock(m_nodes_lock);
            for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); it++)
            {
                nodes.push_back(it->second);
            }
        }

        std::shared_ptr<peer_node> p2p_net_service::get_peer_node(const std::string &id)
        {
            read_lock_guard<rw_lock> lock(m_nodes_lock);

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

            conf_manager *manager = (conf_manager *)g_server->get_module_manager()->get(conf_manager_name).get();
            assert(manager != nullptr);

            //get listen ip and port conf
            std::string host_ip = manager->count("host_ip") ? (*manager)["host_ip"].as<std::string>() : ip::address_v4::any().to_string();
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

            std::string s_port = manager->count("main_net_listen_port") ? (*manager)["main_net_listen_port"].as<std::string>() : DEFAULT_MAIN_NET_LISTEN_PORT;
            val.value() = s_port;
            if (false == port_vdr.validate(val))
            {
                LOG_ERROR << "p2p_net_service init_conf invalid main net port: " << s_port;
                return E_DEFAULT;
            }
            else
            {
                try
                {
                    m_main_net_listen_port = (uint16_t)std::stoi(s_port);
                }
                catch (const std::exception &e)
                {
                    LOG_ERROR << "p2p_net_service init_conf invalid main_port: " << s_port << ", " << e.what();
                    return E_DEFAULT;
                }

            }

            s_port = manager->count("test_net_listen_port") ? (*manager)["test_net_listen_port"].as<std::string>() : DEFAULT_TEST_NET_LISTEN_PORT;
            val.value() = s_port;
            if (false == port_vdr.validate(val))
            {
                LOG_ERROR << "p2p_net_service init_conf invalid test net port: " << s_port;
                return E_DEFAULT;
            }
            else
            {
                try
                {
                    m_test_net_listen_port = (uint16_t)std::stoi(s_port);
                }
                catch (const std::exception &e)
                {
                    LOG_ERROR << "p2p_net_service init_conf invalid test_port: " << s_port << ", " << e.what();
                    return E_DEFAULT;
                }
            }

            //modify by regulus:fix m_test_net_listen_port == m_main_net_listen_pore error.
            if (m_test_net_listen_port == m_main_net_listen_port)
            {
                LOG_ERROR << "p2p_net_service init_conf port config error: main_net_listen_port=test_net_listen_port" << s_port ;
                return E_DEFAULT;
            }
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

            //ipv4 default
            tcp::endpoint ep(ip::address::from_string(m_host_ip), m_main_net_listen_port);

            int32_t ret = E_SUCCESS;

            //main net
            LOG_DEBUG << "p2p net service init main net, ip: " << m_host_ip << " port: " << m_main_net_listen_port;
            ret = CONNECTION_MANAGER->start_listen(ep, &matrix_server_socket_channel_handler::create);
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "p2p net service init main net error, ip: " << m_host_ip << " port: " << m_main_net_listen_port;
                return ret;
            }

            //test net
            ep.port(m_test_net_listen_port);
            LOG_DEBUG << "p2p net service init test net, ip: " << m_host_ip << " port: " << m_test_net_listen_port;
            ret = CONNECTION_MANAGER->start_listen(ep, &matrix_server_socket_channel_handler::create);
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "p2p net service init test net error, ip: " << m_host_ip << " port: " << m_test_net_listen_port;
                return ret;
            }
           
            //ipv6 left to later

            return E_SUCCESS;
        }

        int32_t p2p_net_service::init_connector()
        {
            conf_manager *manager = (conf_manager *)g_server->get_module_manager()->get(conf_manager_name).get();

            if (!manager->count("peer"))
            {
                LOG_DEBUG << "local peer address is empty and will not connect peer nodes by conf";
                return E_DEFAULT;
            }

            //config format: peer address=117.30.51.196:11107
            std::vector<std::string> str_address = (*manager)["peer"].as<std::vector<std::string> >();

            int count = 0;
            ip_validator ip_vdr;
            port_validator port_vdr;
            for (auto it = str_address.begin(); it != str_address.end() && count <DEFAULT_CONNECT_PEER_NODE; it++)
            {
                std::string &addr = *it;
                string_util::trim(addr);
                size_t pos = addr.find(':');
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
                    tcp::endpoint ep(address_v4::from_string(ip), (uint16_t)port);
                    peer_candidate pc = { ep, ns_in_use, 0, time(nullptr), 0 };
                    m_peer_candidates.push_back(std::move(pc));              
					
                    //start connect
                    LOG_DEBUG << "matrix connect peer address, ip: " << addr << " port: " << str_port;
                    int32_t ret = CONNECTION_MANAGER->start_connect(ep, &matrix_client_socket_channel_handler::create);
                    
					if (E_SUCCESS != ret)
                    {
                        LOG_ERROR << "matrix init connector invalid peer address, ip: " << addr << " port: " << str_port;
                        continue;
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

            //init listen
            ret = init_acceptor();
            if (E_SUCCESS != ret)
            {
                return ret;
            }

            //init connect
            init_connector();
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
            TOPIC_MANAGER->subscribe(typeid(cmd_get_peer_nodes_req).name(), [this](std::shared_ptr<message> &msg) {return on_cmd_get_peer_nodes_req(msg); });
            TOPIC_MANAGER->subscribe(P2P_GET_PEER_NODES_REQ, [this](std::shared_ptr<message> &msg) {return send(msg); });
            TOPIC_MANAGER->subscribe(P2P_GET_PEER_NODES_RESP, [this](std::shared_ptr<message> &msg) {return send(msg); });
            TOPIC_MANAGER->subscribe(P2P_NEW_PEER_NODE, [this](std::shared_ptr<message> &msg) { return send(msg); });
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

            //handshake_resp
            invoker = std::bind(&p2p_net_service::on_p2p_new_peer_node, this, std::placeholders::_1);
            m_invokers.insert({ P2P_NEW_PEER_NODE,{ invoker } });
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

            static uint32_t tick_count = 0;
            ++tick_count;//one min
            
            on_timer_check_peer_candidate();

            if (tick_count % TIMER_INTERV_MIN_P2P_PEER_INFO_EXCHANGE == 0)
            {
                tick_count = 0;
                on_timer_peer_info_exchange();
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
            //check up-bound
            uint32_t in_use_peer_cnt = 0;
            for (std::list<peer_candidate>::iterator it = m_peer_candidates.begin(); it != m_peer_candidates.end(); ++it)
            {
                if (ns_in_use == it->net_st)
                {
                    in_use_peer_cnt++;
                }
            }
            if (in_use_peer_cnt >= max_connected_cnt)
            {
                return E_SUCCESS;
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
                    //connect
                    it->last_conn_tm = time(nullptr);
                    it->net_st = ns_in_use;
                    it->reconn_cnt++;
                    try
                    {
                        LOG_DEBUG << "matrix connect peer address, ip: " << it->tcp_ep.address() << " port: " << it->tcp_ep.port();
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

                    if (new_conn_cnt > max_connected_cnt / 4)
                    {
                        break;//not too many conn at a time
                    }
                }
            }

            return E_SUCCESS;
        }

        bool p2p_net_service::add_peer_node(const socket_id &sid, const std::string &nid, int32_t core_version, int32_t protocol_version)
        {
            std::shared_ptr<peer_node> node = std::make_shared<peer_node>();
            node->m_id = nid;
            node->m_sid = sid;
            node->m_core_version = core_version;
            node->m_protocol_version = protocol_version;
            node->m_connected_time = std::time(nullptr);
            node->m_live_time = 0;
            node->m_connection_status = connected;
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
                node->m_peer_addr = ptr_tcp_ch->get_remote_addr();
                node->m_local_addr = ptr_tcp_ch->get_local_addr();
            }
            else
            {
                LOG_ERROR << nid << "not find in connected channels.";
            }
            
            write_lock_guard<rw_lock> lock(m_nodes_lock);
            m_peer_nodes_map.insert(std::make_pair(node->m_id, node));

            return true;
        }

        void p2p_net_service::remove_peer_node(const std::string &id)
        {
            write_lock_guard<rw_lock> lock(m_nodes_lock);
            m_peer_nodes_map.erase(id);
        }

        int32_t p2p_net_service::on_ver_req(std::shared_ptr<message> &msg)
        {
            std::shared_ptr<matrix::service_core::ver_req> req_content = std::dynamic_pointer_cast<matrix::service_core::ver_req>(msg->content);
            assert(nullptr != req_content);
            //filter node which connects to itself
            if (req_content->body.__isset.node_id)
            {
                if (req_content->body.node_id == CONF_MANAGER->get_node_id())
                {
                    auto ch = CONNECTION_MANAGER->get_channel(msg->header.src_sid);
                    if (ch)
                    {
                        auto tcp_ch = std::dynamic_pointer_cast<tcp_socket_channel>(ch);
                        if (tcp_ch)
                        {
                             std::list<peer_candidate>::iterator it = std::find_if( m_peer_candidates.begin(), m_peer_candidates.end()
                                , [=](peer_candidate& pc) -> bool { return tcp_ch->get_remote_addr() == pc.tcp_ep; } );
                            if (it != m_peer_candidates.end())
                            {
                                it->last_conn_tm = time(nullptr);
                                it->net_st = ns_zombie;
                            }                           
                        }
                        //close channel
                        ch->stop();
                        return E_SUCCESS;
                    }
                }
            }

            LOG_DEBUG << "p2p net service received ver req, node id: " << req_content->body.node_id;

            std::shared_ptr<message> resp_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::ver_resp> resp_content = std::make_shared<matrix::service_core::ver_resp>();

            //header
            resp_content->header.magic = TEST_NET;
            resp_content->header.msg_name = VER_RESP;
            resp_content->header.__set_nonce(id_generator().generate_nonce());

            //body
            resp_content->body.node_id = CONF_MANAGER->get_node_id();
            resp_content->body.core_version = CORE_VERSION;
            resp_content->body.protocol_version = PROTOCO_VERSION;

            resp_msg->set_content(std::dynamic_pointer_cast<base>(resp_content));
            resp_msg->set_name(VER_RESP);
            resp_msg->header.dst_sid = msg->header.src_sid;

            CONNECTION_MANAGER->send_message(resp_msg->header.dst_sid, resp_msg);

            //add new peer node
            if (!add_peer_node(msg->header.src_sid, req_content->body.node_id, req_content->body.core_version, req_content->body.protocol_version))
            {
                LOG_ERROR << "add node( " << req_content->body.node_id << " ) failed.";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_ver_resp(std::shared_ptr<message> &msg)
        {
            std::shared_ptr<matrix::service_core::ver_resp> resp_content = std::dynamic_pointer_cast<matrix::service_core::ver_resp>(msg->content);

            LOG_DEBUG << "p2p net service received ver resp, node id: " << resp_content->body.node_id;

            //add new peer node
            if (!add_peer_node(msg->header.src_sid, resp_content->body.node_id, resp_content->body.core_version, resp_content->body.protocol_version))
            {
                LOG_ERROR << "add node( " << resp_content->body.node_id << " ) failed.";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_tcp_channel_error(std::shared_ptr<message> &msg)
        {
            socket_id  sid = msg->header.src_sid;
            LOG_ERROR << "p2p net service received tcp channel error msg, " << sid.to_string();

            //if client, connect next
            if (CLIENT_SOCKET == sid.get_type())
            {
                //find peer candidate
                std::shared_ptr<tcp_socket_channel_error_msg> err_msg = std::dynamic_pointer_cast<tcp_socket_channel_error_msg>(msg);
                if (err_msg)
                {
                    std::list<peer_candidate>::iterator it = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end()
                        , [=](peer_candidate& pc) -> bool { return err_msg->ep == pc.tcp_ep; });
                    if (it != m_peer_candidates.end())
                    {
                        it->last_conn_tm = time(nullptr);
                        it->net_st = ns_failed;
                    }
                    else
                    {
                        assert(0);
                        LOG_ERROR << "a peer_node network error occurs, but not in ip candidates: " << err_msg->ep.address() << ":" << err_msg->ep.port();
                    }
                }
            }
            else
            {
                //
            }

            //rm peer_mode
            for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); ++it)
            {
                if (it->second->m_sid == sid)
                {
                    m_peer_nodes_map.erase(it);
                    break;
                }
            }

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
                    it->last_conn_tm = time(nullptr);
                    it->net_st = ns_in_use;
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
                req_content->header.magic = TEST_NET;
                req_content->header.msg_name = VER_REQ;
                req_content->header.__set_nonce(id_generator().generate_nonce());

                //body
                req_content->body.node_id = CONF_MANAGER->get_node_id();
                req_content->body.core_version = CORE_VERSION;
                req_content->body.protocol_version = PROTOCO_VERSION;
                req_content->body.time_stamp = std::time(nullptr);
                req_content->body.addr_me.ip = get_host_ip();
                req_content->body.addr_me.port = get_main_net_listen_port();

                tcp::endpoint ep = std::dynamic_pointer_cast<client_tcp_connect_notification>(msg)->ep;
                req_content->body.addr_you.ip = ep.address().to_string();
                req_content->body.addr_you.port = ep.port();
                req_content->body.start_height = 0;              //later

                LOG_INFO << "me(" << req_content->body.addr_me.ip << ":" << req_content->body.addr_me.port << ") connect to peer(" << req_content->body.addr_you.ip << ":" << req_content->body.addr_you.port << ")";
                
                req_msg->set_content(std::dynamic_pointer_cast<base>(req_content));
                req_msg->set_name(VER_REQ);
                req_msg->header.dst_sid = msg->header.src_sid;

                CONNECTION_MANAGER->send_message(req_msg->header.dst_sid, req_msg);

                return E_SUCCESS;
            }
            else
            {
                //update peer candidate info
                if (it != m_peer_candidates.end())
                {
                    it->last_conn_tm = time(nullptr);
                    it->net_st = ns_failed;
                }
                else
                {
                    LOG_WARNING << "a client tcp connection failed, but not in peer candidate: " << notification_content->ep.address() << ":" << notification_content->ep.port();
                }

                //cancel connect and connect next        

                return E_SUCCESS;
            }
        }

        int32_t p2p_net_service::on_cmd_get_peer_nodes_req(std::shared_ptr<message> &msg)
        {
            std::shared_ptr<ai::dbc::cmd_get_peer_nodes_resp> cmd_resp = std::make_shared<ai::dbc::cmd_get_peer_nodes_resp>();
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";
            {
                read_lock_guard<rw_lock> lock(m_nodes_lock);
                for (auto itn = m_peer_nodes_map.begin(); itn != m_peer_nodes_map.end(); ++itn)
                {
                    ai::dbc::cmd_peer_node_info node_info;
                    node_info.peer_node_id = itn->second->m_id;
                    node_info.live_time_stamp = itn->second->m_live_time;
                    node_info.addr.ip = itn->second->m_peer_addr.get_ip();
                    node_info.addr.port = itn->second->m_peer_addr.get_port();
                    node_info.service_list.clear();
                    node_info.service_list.push_back(std::string("ai_training"));
                    cmd_resp->peer_nodes_list.push_back(std::move(node_info));
                }
            }

            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_get_peer_nodes_resp).name(), cmd_resp);

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_get_peer_nodes_req(std::shared_ptr<message> &msg)
        {
            if (m_peer_nodes_map.size() == 0)
            {
                //ignore request
                return E_SUCCESS;
            }

            const uint32_t max_peer_cnt_per_pack = 50;
            const uint32_t max_pack_cnt = 10;            
            read_lock_guard<rw_lock> lock(m_nodes_lock);
            auto it = m_peer_nodes_map.begin();
            for (uint32_t i = 0; (i <= m_peer_nodes_map.size() / max_peer_cnt_per_pack) && (i < max_pack_cnt); i++)
            {
                //common header
                std::shared_ptr<message> resp_msg = std::make_shared<message>();
                resp_msg->header.msg_name = P2P_GET_PEER_NODES_RESP;
                resp_msg->header.msg_priority = 0;
                resp_msg->header.dst_sid = msg->header.src_sid;
                auto resp_content = std::make_shared<matrix::service_core::get_peer_nodes_resp>();
                //header
                resp_content->header.magic = TEST_NET;
                resp_content->header.msg_name = P2P_GET_PEER_NODES_RESP;
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
                    info.peer_node_id = it->second->m_id;
                    info.live_time_stamp = (int32_t)it->second->m_live_time;
                    info.addr.ip = it->second->m_peer_addr.get_ip();
                    info.addr.port = it->second->m_peer_addr.get_port();
                    info.service_list.push_back(std::string("ai_training"));
                    resp_content->body.peer_nodes_list.push_back(std::move(info));
                    ++peer_cnt;
                }

                resp_msg->set_content(std::dynamic_pointer_cast<matrix::core::base>(resp_content));

                CONNECTION_MANAGER->send_message(msg->header.src_sid, resp_msg);
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_get_peer_nodes_resp(std::shared_ptr<message> &msg)
        {
            std::shared_ptr<matrix::service_core::get_peer_nodes_resp> rsp = std::dynamic_pointer_cast<matrix::service_core::get_peer_nodes_resp>(msg->content);
            for (auto it = rsp->body.peer_nodes_list.begin(); it != rsp->body.peer_nodes_list.end(); ++it)
            {
                if (it->addr.ip.empty() || it->addr.port <= 0)
                {
                    LOG_DEBUG << "recv a peer with empty addr: " << it->peer_node_id;
                    continue;
                }
                tcp::endpoint ep(address_v4::from_string(it->addr.ip), (uint16_t)it->addr.port);
                //is in list
                std::list<peer_candidate>::iterator it_pc = std::find_if(m_peer_candidates.begin(), m_peer_candidates.end()
                    , [=](peer_candidate& pc) -> bool { return ep == pc.tcp_ep; });
                if (it_pc == m_peer_candidates.end())
                {
                    peer_candidate pc = { ep, ns_idle, 0, time(nullptr), 0 };
                    m_peer_candidates.push_back(std::move(pc));
                }
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_p2p_new_peer_node(std::shared_ptr<message> &msg)
        {
            /*if (!msg)
            {
                LOG_ERROR << "p2p net service on p2p new peer node null msg";
                return E_DEFAULT;
            }

            using msg_new_node = matrix::service_core::msg_new_peer_node;
            std::shared_ptr<msg_new_node> msg_node = std::dynamic_pointer_cast<msg_new_node>(msg->get_content());
            if (!msg_node)
            {
                LOG_ERROR << "p2p net service on p2p new peer node null msg node";
                return E_DEFAULT;
            }

            if (!add_peer_node(msg_node->sid, msg_node->node_id))
            {
                LOG_ERROR << "add node(" << msg_node->node_id << ") failed.";
                return E_DEFAULT;
            }*/

            return E_SUCCESS;
        }

        int32_t p2p_net_service::send_get_peer_nodes()
        {
            std::shared_ptr<matrix::service_core::get_peer_nodes_req> req_content = std::make_shared<matrix::service_core::get_peer_nodes_req>();
            req_content->header.magic = TEST_NET;
            req_content->header.msg_name = P2P_GET_PEER_NODES_REQ;
            req_content->header.__set_nonce(id_generator().generate_nonce());

            std::shared_ptr<message> req_msg = std::make_shared<message>();
            req_msg->set_name(P2P_GET_PEER_NODES_REQ);
            req_msg->set_content(std::dynamic_pointer_cast<base> (req_content));
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
            resp_content->header.magic = TEST_NET;
            resp_content->header.msg_name = P2P_GET_PEER_NODES_RESP;
            resp_content->header.__set_nonce(id_generator().generate_nonce());

            if (node)//broadcast one node
            {
                //body
                matrix::service_core::peer_node_info info;
                info.peer_node_id = node->m_id;
                info.live_time_stamp = (int32_t)node->m_live_time;
                info.addr.ip = node->m_peer_addr.get_ip();
                info.addr.port = node->m_peer_addr.get_port();
                info.service_list.push_back(std::string("ai_training"));
                resp_content->body.peer_nodes_list.push_back(std::move(info));
                resp_msg->set_content(std::dynamic_pointer_cast<matrix::core::base>(resp_content));

                CONNECTION_MANAGER->broadcast_message(resp_msg);
            }
            else// broadcast all nodes
            {
                {
                    read_lock_guard<rw_lock> lock(m_nodes_lock);
                    for (auto it = m_peer_nodes_map.begin(); it != m_peer_nodes_map.end(); ++it)
                    {
                        if (it->second->m_id == CONF_MANAGER->get_node_id())
                        {
                            assert(0); //should never occur
                            LOG_WARNING << "peer node(" << it->second->m_id << ") is myself.";
                            continue;
                        }                            
                        matrix::service_core::peer_node_info info;
                        info.peer_node_id = it->second->m_id;
                        info.live_time_stamp = (int32_t)it->second->m_live_time;
                        info.addr.ip = it->second->m_peer_addr.get_ip();
                        info.addr.port = it->second->m_peer_addr.get_port();
                        info.service_list.push_back(std::string("ai_training"));
                        resp_content->body.peer_nodes_list.push_back(std::move(info));
                    }
                }

                if (resp_content->body.peer_nodes_list.size() > 0)
                {
                    resp_msg->set_content(std::dynamic_pointer_cast<matrix::core::base>(resp_content));
                    CONNECTION_MANAGER->broadcast_message(resp_msg);
                }
                else
                {
                    return E_DEFAULT;
                }
            }

            return E_SUCCESS;
        }

    }
}