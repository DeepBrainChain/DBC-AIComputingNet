/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : connection_manager_test.cpp
* description       : 
* date              : 2018/7/8
* author            : Jimmy Kuang
**********************************************************************************/


// The following two lines indicates boost test with Shared Library mode
//#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>


#include "network/connection_manager.h"
#include "tcp_socket_channel.h"
#include "error.h"

using namespace boost::unit_test;
using namespace std;
using namespace matrix::core;

class mock_tcp_socket_channel:public tcp_socket_channel
{
public:
    mock_tcp_socket_channel(ios_ptr ios, socket_id sid, handler_create_functor func, int32_t len=0):
            tcp_socket_channel(ios,sid,func,len)
    {

    }

    int32_t start()
    {
        m_socket_handler = m_handler_functor(shared_from_this());
        return 0;
    }


};




class mock_socket_channel_handler: public socket_channel_handler
{
public:

    virtual ~mock_socket_channel_handler() {}
    mock_socket_channel_handler () {}
    //mock_socket_channel_handler(std::shared_ptr<channel> ch):m_channel(ch) {}

    virtual int32_t start() {
        return 0;
    }

    virtual int32_t stop() {return 0;}

public:

    virtual int32_t on_read(channel_handler_context &ctx, byte_buf &in) {return 0;}

    virtual int32_t on_write(channel_handler_context &ctx, message &msg, byte_buf &buf) {return 0;}

    virtual int32_t on_error() {return 0;}

public:

    virtual int32_t on_before_msg_send(message &msg) {return 0;}

    virtual int32_t on_after_msg_sent(message &msg) {return 0;}

    virtual int32_t on_before_msg_receive() {return 0;}

    virtual int32_t on_after_msg_received(message &msg) {return 0;}

    virtual bool is_logined() {return true;}

    static std::shared_ptr<socket_channel_handler> create(std::shared_ptr<channel> ch)
    {
        shared_ptr<socket_channel_handler> handler(new mock_socket_channel_handler());
        return handler->shared_from_this();
    }

};

BOOST_AUTO_TEST_CASE(test_find_fast_path_ok) {

    connection_manager cm;

    bpo::variables_map v;

    //cm.init(v);

    auto context = std::make_shared<boost::asio::io_context>();
    auto ch = std::make_shared<mock_tcp_socket_channel>(context,socket_id(SERVER_SOCKET,1),&mock_socket_channel_handler::create);
    ch->start();

    ch->set_remote_node_id("2gfpp3MAB3zWhesVATX5Y4Ttd2wXf3caoFnqzEmZeet");
    cm.add_channel(socket_id(SERVER_SOCKET,1), ch);

    vector<string> path;
    path.push_back("2gfpp3MAB4Jo9SaiLrg68Po5WvJKt1rSjzsT9uSpxL3");
    path.push_back("2gfpp3MAB4ACimTtKJtNUj2cGzaZhcwcg3NuNUmgZTT");
    path.push_back("2gfpp3MAB3zWhesVATX5Y4Ttd2wXf3caoFnqzEmZeet");


    auto c = cm.find_fast_path(path);


    BOOST_TEST_REQUIRE(c != nullptr);
    BOOST_TEST(c->id().get_id() == 1);

    BOOST_TEST(path.size() == 2);
}

BOOST_AUTO_TEST_CASE(test_find_fast_path_ok_short_cut) {

    connection_manager cm;

    bpo::variables_map v;

    auto context = std::make_shared<boost::asio::io_context>();

    auto ch = std::make_shared<mock_tcp_socket_channel>(context,socket_id(SERVER_SOCKET,1),&mock_socket_channel_handler::create);
    ch->start();

    ch->set_remote_node_id("2gfpp3MAB4ACimTtKJtNUj2cGzaZhcwcg3NuNUmgZTT");
    cm.add_channel(socket_id(SERVER_SOCKET,1), ch);

    ch = std::make_shared<mock_tcp_socket_channel>(context,socket_id(SERVER_SOCKET,2),&mock_socket_channel_handler::create);
    ch->start();

    ch->set_remote_node_id("2gfpp3MAB4Jo9SaiLrg68Po5WvJKt1rSjzsT9uSpxL3");
    cm.add_channel(socket_id(SERVER_SOCKET,2), ch);

    vector<string> path;
    path.push_back("2gfpp3MAB4Jo9SaiLrg68Po5WvJKt1rSjzsT9uSpxL3");
    path.push_back("2gfpp3MAB4ACimTtKJtNUj2cGzaZhcwcg3NuNUmgZTT");
    path.push_back("2gfpp3MAB3zWhesVATX5Y4Ttd2wXf3caoFnqzEmZeet");


    auto c = cm.find_fast_path(path);


    BOOST_TEST_REQUIRE(c != nullptr);
    BOOST_TEST(c->id().get_id() == 2);

    BOOST_TEST(path.size() == 0);
}

BOOST_AUTO_TEST_CASE(test_find_fast_path_no_match_path) {

    connection_manager cm;

    bpo::variables_map v;

    //cm.init(v);

    auto context = std::make_shared<boost::asio::io_context>();
    auto ch = std::make_shared<mock_tcp_socket_channel>(context,socket_id(SERVER_SOCKET,1),&mock_socket_channel_handler::create);
    ch->start();

    ch->set_remote_node_id("2gfpp3MAB4ACimTtKJtNUj2cGzaZhcwcg3NuNUmgZHH");
    cm.add_channel(socket_id(SERVER_SOCKET,1), ch);

    vector<string> path;
    path.push_back("2gfpp3MAB4Jo9SaiLrg68Po5WvJKt1rSjzsT9uSpxL3");
    path.push_back("2gfpp3MAB4ACimTtKJtNUj2cGzaZhcwcg3NuNUmgZTT");
    path.push_back("2gfpp3MAB3zWhesVATX5Y4Ttd2wXf3caoFnqzEmZeet");


    auto ch1 = cm.find_fast_path(path);

    BOOST_TEST_REQUIRE(ch1 == nullptr);
    BOOST_TEST(path.size() == 3);
}

BOOST_AUTO_TEST_CASE(test_find_fast_path_no_live_connection) {

    connection_manager cm;

    bpo::variables_map v;

    vector<string> path;
    path.push_back("2gfpp3MAB4Jo9SaiLrg68Po5WvJKt1rSjzsT9uSpxL3");
    path.push_back("2gfpp3MAB4ACimTtKJtNUj2cGzaZhcwcg3NuNUmgZTT");
    path.push_back("2gfpp3MAB3zWhesVATX5Y4Ttd2wXf3caoFnqzEmZeet");


    auto ch1 = cm.find_fast_path(path);

    BOOST_TEST_REQUIRE(ch1 == nullptr);
    BOOST_TEST(path.size() == 3);
}