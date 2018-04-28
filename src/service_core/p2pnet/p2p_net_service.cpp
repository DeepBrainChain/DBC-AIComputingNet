/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºp2p_net_service.cpp
* description    £ºp2p net service
* date                  : 2018.01.28
* author            £ºBruce Feng
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


using namespace std;
using namespace boost::asio::ip;
using namespace matrix::core;


namespace matrix
{
    namespace service_core
    {
        int32_t p2p_net_service::init_conf()
        {
            variable_value val;
            ip_validator ip_vdr;
            port_validator port_vdr;

            conf_manager *manager = (conf_manager *)g_server->get_module_manager()->get(conf_manager_name).get();
            assert(manager != nullptr);

            //get listen ip and port conf
            std::string host_ip  = manager->count("host_ip") ? (*manager)["host_ip"].as<std::string>() : ip::address_v4::any().to_string();
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
            std::vector<std::string> str_address = (*manager)["peer"].as<std::vector<std::string>>();

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
                    m_peer_addresses.push_back(ep);
                    //start connect
                    LOG_DEBUG << "matrix connect peer address, ip: " << addr << " port: " << str_port;
                    int32_t ret = CONNECTION_MANAGER->start_connect(ep, &matrix_client_socket_channel_handler::create);
                    if (E_SUCCESS != ret)
                    {
                        LOG_ERROR << "matrix init connector invalid peer address, ip: " << addr << " port: " << str_port;
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
            //init topic
            init_subscription();

            //init invoker
            init_invoker();

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
        }

        void p2p_net_service::init_invoker()
        {
            invoker_type invoker;

            //tcp channel error
            invoker = std::bind(&p2p_net_service::on_tcp_channel_error, this, std::placeholders::_1);
            m_invokers.insert({ TCP_CHANNEL_ERROR, { invoker } });

            //client tcp connect success
            invoker = std::bind(&p2p_net_service::on_client_tcp_connect_notification, this, std::placeholders::_1);
            m_invokers.insert({ CLIENT_CONNECT_NOTIFICATION, { invoker } });

            //ver req
            invoker = std::bind(&p2p_net_service::on_ver_req, this, std::placeholders::_1);
            m_invokers.insert({ VER_REQ, { invoker } });

            //ver resp
            invoker = std::bind(&p2p_net_service::on_ver_resp, this, std::placeholders::_1);
            m_invokers.insert({ VER_RESP, { invoker } });
        }

        int32_t p2p_net_service::on_time_out(std::shared_ptr<core_timer> timer)
        {
            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_ver_req(std::shared_ptr<message> &msg)
        {
            std::shared_ptr<message> resp_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::ver_resp> resp_content = std::make_shared<matrix::service_core::ver_resp>();

            //header
            resp_content->header.length = 0;
            resp_content->header.magic = TEST_NET;
            resp_content->header.msg_name = VER_RESP;
            resp_content->header.check_sum = 0;
            resp_content->header.session_id = 0;

            //body
            resp_content->body.version = PROTOCO_VERSION;

            resp_msg->set_content(std::dynamic_pointer_cast<base>(resp_content));
            resp_msg->set_name(VER_RESP);
            resp_msg->header.dst_sid = msg->header.src_sid;

            CONNECTION_MANAGER->send_message(resp_msg->header.dst_sid, resp_msg);
            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_ver_resp(std::shared_ptr<message> &msg)
        {
            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_tcp_channel_error(std::shared_ptr<message> &msg)
        {
            socket_id  sid = msg->header.src_sid;
            LOG_ERROR << "p2p net service received tcp channel error msg, " << sid.to_string();

            //if client, connect next
            if (CLIENT_SOCKET == sid.get_type())
            {

            }
            else
            {
                //
            }

            return E_SUCCESS;
        }

        int32_t p2p_net_service::on_client_tcp_connect_notification(std::shared_ptr<message> &msg)
        {
            auto notification_content = std::dynamic_pointer_cast<client_tcp_connect_notification>(msg);

            if (CLIENT_CONNECT_SUCCESS == notification_content->status)
            {
                //create ver_req message
                std::shared_ptr<message> req_msg = std::make_shared<message>();
                std::shared_ptr<matrix::service_core::ver_req> req_content = std::make_shared<matrix::service_core::ver_req>();

                //header
                req_content->header.length = 0;
                req_content->header.magic = TEST_NET;
                req_content->header.msg_name = VER_REQ;
                req_content->header.check_sum = 0;
                req_content->header.session_id = 0;

                //body
                req_content->body.version = PROTOCO_VERSION;
                req_content->body.time_stamp = std::time(nullptr);
                req_content->body.addr_me.ip = g_server->get_p2p_net_service()->get_host_ip();
                req_content->body.addr_me.port = g_server->get_p2p_net_service()->get_main_net_listen_port();

                tcp::endpoint ep = std::dynamic_pointer_cast<client_tcp_connect_notification>(msg)->ep;
                req_content->body.addr_you.ip = ep.address().to_string();
                req_content->body.addr_you.port = ep.port();
                req_content->body.nonce = 0;                        //later
                req_content->body.start_height = 0;              //later

                req_msg->set_content(std::dynamic_pointer_cast<base>(req_content));
                req_msg->set_name(VER_REQ);
                req_msg->header.dst_sid = msg->header.src_sid;

                CONNECTION_MANAGER->send_message(req_msg->header.dst_sid, req_msg);
                return E_SUCCESS;
            }
            else
            {
                //cancel connect and connect next
                return E_SUCCESS;
            } 
        }
    }

}