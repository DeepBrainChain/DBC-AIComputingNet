/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : connection_manager_test.cpp
* description       : 
* date              : 2018/8/30
* author            : Taz Zhang
**********************************************************************************/

#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include "network/connection_manager.h"
#include "tcp_socket_channel.h"
#include <string>
#include <functional>
#include <chrono>
#include <boost/program_options.hpp>
#include <core/network/tcp_socket_channel.h>
#include "log.h"
#include "server_initiator.h"
#include "server.h"
#include "crypto_service.h"
#include "timer_matrix_manager.h"
#include "service_message_id.h"

using namespace std::chrono;

using namespace boost::unit_test;
using namespace boost::asio::ip;
using namespace std;
using namespace matrix::core;

#define LOAD_MODULE(name)\
mdl = std::dynamic_pointer_cast<module>(std::make_shared<name>());\
LOG_INFO << "begin to init "<<mdl->module_name();\
g_server->get_module_manager()->add_module(mdl->module_name(), mdl);\
ret = mdl->init(vm);\
if (E_SUCCESS != ret)\
{\
break;\
}\
mdl->start();\
\



class connection_manager_mock : public connection_manager
{
public:
    int32_t get_connect_num()
    {
        return connection_manager::get_connect_num();
    }

    int32_t get_out_connect_num()
    {
        return connection_manager::get_out_connect_num();
    }

    int32_t get_in_connect_num()
    {
        return connection_manager::get_in_connect_num();
    }
};

class dbc_server_initiator_mock : public matrix::core::server_initiator
{
public:
    static server_initiator *create_initiator() { return new dbc_server_initiator_mock(); }

    dbc_server_initiator_mock() {}

    ~dbc_server_initiator_mock() {}

    virtual int32_t exit() { return E_SUCCESS; }

    virtual int32_t parse_command_line(int argc, const char *const argv[],
                                       boost::program_options::variables_map &vm) { return E_SUCCESS; }

    virtual int32_t on_cmd_init() { return E_SUCCESS; }

    virtual int32_t on_daemon() { return E_SUCCESS; }

    virtual int32_t init(int argc, char *argv[])
    {
        int32_t ret = E_SUCCESS;
        variables_map vm;
        load_vm(vm);
        std::shared_ptr<matrix::core::module> mdl(nullptr);
        do
        {
            LOAD_MODULE(crypto_service)
            LOAD_MODULE(env_manager)
            LOAD_MODULE(conf_manager)
            LOAD_MODULE(topic_manager)
            LOAD_MODULE(timer_matrix_manager)
            LOAD_MODULE(connection_manager_mock)
            return E_SUCCESS;
        } while (0);       
        return E_EXIT_FAILURE;
    }

    void load_vm(bpo::variables_map &_variables_map)
    {
        {
            variable_value val;
            val.value() = 128;
            _variables_map.insert({"max_connect", val});
        }
        {
            variable_value val;
            val.value() = "41107";
            _variables_map.insert({"main_net_listen_port", val});
        }

        {
            variable_value val;
            val.value() = "31107";
            _variables_map.insert({"test_net_listen_port", val});
        }


        {
            variable_value val;
            val.value() = "";
            _variables_map.insert({"peer", val});
        }

    }
};


namespace TestNetwork
{
    static high_resolution_clock::time_point send_start_time;
    static high_resolution_clock::time_point recv_start_time;

    static constexpr uint32_t stringlen1 = 12;
    static char string1[stringlen1] = {0};

    static constexpr uint32_t stringlen2 = 2048;
    static char string2[stringlen2] = {0};

    static constexpr uint32_t stringlen3 = 10240;
    static char string3[stringlen3] = {0};

    static constexpr uint32_t stringlen4 = 1024 * 512;
    static char string4[stringlen4] = {0};

    static uint32_t test_conn_max = 200;
    static constexpr uint32_t conn_max = 121;

    enum TEST_STEP
    {
        CLI_BEGIN = 0,
        CLI_CONNECT,
        CLI_RECONNECT,
        CLI_SEND_MORE_PACKATE,
        CLI_RECV_MORE_PACKATE,
        CLI_DISCONNECT,
        CLI_READ_WRITE_MULTI_PACKATE,
        CLI_SEND_NULL_DATA,
        CLI_CONCURRENT_CONNECT,
        CLI_CLOSE,
        CLI_END,

        SVR_BEGIN,
        SVR_LISTEN,
        SVR_ACCEPT,
        SVR_BROADCAST,
        SVR_END,
    };



    
    static std::vector<TEST_STEP> g_steppath;
    static rw_lock g_steppath_lock;

    static std::atomic<int> g_iConnNum(0);
    static std::mutex g_iConnNumLock;

    static std::atomic<int> g_iWriteCnt(0);
    static std::atomic<int> g_iReadCnt(0);
    static std::atomic<int> g_iConnectResult(0);
    static std::atomic<int> g_iReConnectResult(0);
    static std::atomic<int> g_iDisconnectResult(0);

    static std::atomic<int> g_iSendFullCnt(0);
    static std::atomic<int> g_iSendEnd(0);
 

    static std::atomic<int> g_iOnWriteCnt(0);

    static std::atomic<int> g_iSendLen(0);
    static std::atomic<int> g_iRecvLen(0);

    static std::atomic<int> g_iDisconnect(0);

    static std::atomic<int> g_iTestEnd(0);
    static std::atomic<int> g_connect_num(0);
    static std::atomic<int> g_out_connect_num(0);
    static std::atomic<int> g_in_connect_num(0);
    static std::atomic<int> g_connection_num(0);

    static std::atomic<int> g_client_error_num(0);
    static std::atomic<int> g_client_error_num2(0);
    static std::atomic<int> g_client_active_num(0);
    static std::atomic<int> g_client_active_num2(0);
    static std::atomic<int> g_client_fail_num(0);
    static std::atomic<int> g_client_fail_num2(0);
    static time_t g_last_step_timestamp = time(NULL);
    static TEST_STEP g_iTestStep;
    static std::string g_strEndInfo;


    static TEST_STEP step()
    {
        return g_iTestStep;
    }


    static uint32_t get_valid_conn_num()
    {
        if (test_conn_max > conn_max)
        {
            return conn_max;
        }
        else
        {
            return test_conn_max;
        }
    }


    static void set_end(std::string endinfo)
    {
        g_iTestEnd = 1;
        g_strEndInfo = endinfo;
    }

    static const char *step_str(TEST_STEP step)
    {
        switch (step)
        {
            case TestNetwork::CLI_BEGIN:
                return "CLI_BEGIN";
            case TestNetwork::CLI_END:
                return "CLI_END";
            case TestNetwork::CLI_SEND_NULL_DATA:
                return "CLI_SEND_NULL_DATA";
            case TestNetwork::CLI_DISCONNECT:
                return "CLI_DISCONNECT";
            case TestNetwork::CLI_CONNECT:
                return "CLI_CONNECT";
            case TestNetwork::CLI_RECONNECT:
                return "CLI_RECONNECT";
            case TestNetwork::CLI_SEND_MORE_PACKATE:
                return "CLI_SEND_MORE_PACKATE";
            case TestNetwork::CLI_RECV_MORE_PACKATE:
                return "CLI_RECV_MORE_PACKATE";
            case TestNetwork::CLI_READ_WRITE_MULTI_PACKATE:
                return "CLI_READ_WRITE_MULTI_PACKATE";
            case TestNetwork::CLI_CONCURRENT_CONNECT:
                return "CLI_CONCURRENT_CONNECT";
            case TestNetwork::CLI_CLOSE:
                return "CLI_CLOSE";
            case TestNetwork::SVR_BEGIN:
                return "SVR_BEGIN";
            case TestNetwork::SVR_LISTEN:
                return "SVR_LISTEN";
            case TestNetwork::SVR_ACCEPT:
                return "SVR_ACCEPT";
            case TestNetwork::SVR_BROADCAST:
                return "SVR_BROADCAST";
            case TestNetwork::SVR_END:
                return "SVR_END";
            default:
                return "unknown";
        }
    }

    static void add_conn_num()
    {
        std::unique_lock<std::mutex> lock(g_iConnNumLock);
        g_iConnNum += 1;
    }


    static void init_strings()
    {
        memset(string1, '1', stringlen1);
        memset(string2, '2', stringlen2);
        memset(string3, '3', stringlen3);
        memset(string4, '4', stringlen4);
    }

    static const char *get_string(int32_t index, uint32_t &stringlen)
    {
        switch (index)
        {
            case 0:
                stringlen = 5;
                return "123456";
            case 1:
                stringlen = stringlen1;
                return string1;
            case 2:
                stringlen = stringlen2;
                return string2;
            case 3:
                stringlen = stringlen3;
                return string3;
            case 4:
                stringlen = stringlen4;
                return string4;
            default:
                stringlen = stringlen2;
                return string2;
        }

    }

    static bool string_cmp(int32_t index, const char *string, uint32_t stringlen)
    {
        assert(index >= 1 && index <= 4);
        switch (index)
        {
            case 1:
                if (stringlen == stringlen1)
                {
                    return 0 == memcmp(string1, string, stringlen1);
                }
            case 2:
                if (stringlen == stringlen2)
                {
                    return 0 == memcmp(string2, string, stringlen2);
                }
            case 3:
                if (stringlen == stringlen3)
                {
                    return 0 == memcmp(string3, string, stringlen3);
                }
            case 4:
                if (stringlen == stringlen4)
                {
                    return 0 == memcmp(string4, string, stringlen4);
                }
        }
        return false;
    }

    static void begin()
    {
        init_strings();
        g_steppath.clear();
        g_iConnNum = 0;
        g_iWriteCnt = 0;
        g_iReadCnt = 0;
        g_iConnectResult = 0;
        g_iReConnectResult = 0;
        g_iDisconnectResult = 0;
        g_iSendFullCnt = 0;
        g_iSendEnd = 0;
        g_iSendLen = 0;
        g_iRecvLen = 0;
        g_iDisconnect = 0;
        g_iTestEnd = 0;
        g_connect_num = 0;
        g_out_connect_num = 0;
        g_in_connect_num = 0;
        g_connection_num = 0;
        g_client_error_num = 0;
        g_client_error_num2 = 0;
        g_client_active_num = 0;
        g_client_active_num2 = 0;
        g_client_fail_num = 0;
        g_client_fail_num2 = 0;
        g_strEndInfo="";
        g_iOnWriteCnt=0;
        

    }


    static std::string path_str()
    {
        std::string path;
        for (int j = 0; j < (int) TestNetwork::g_steppath.size(); ++j)
        {
            path += "->" + std::string(TestNetwork::step_str(TestNetwork::g_steppath.at(j)));
        }

        return path;
    }

    static void to_step(TEST_STEP s)
    {
        write_lock_guard<rw_lock> lock(g_steppath_lock);
        g_iTestStep = s;
        g_steppath.push_back(s);
        LOG_INFO << "test_step_path:" <<  TestNetwork::path_str();
        g_last_step_timestamp = time(NULL);
    }


    static void end()
    {
        high_resolution_clock::time_point _now_time = high_resolution_clock::now();
        auto time_span_ms1 = duration_cast<milliseconds>(_now_time - recv_start_time);
        auto time_span_ms2 = duration_cast<milliseconds>(_now_time - send_start_time);

        float send_speed = g_iSendLen / time_span_ms1.count();
        float recv_speed = g_iRecvLen / time_span_ms2.count();

        LOG_INFO << "--==============----TEST RESULT----============--";
        LOG_INFO << "test_step_path:" <<  TestNetwork::path_str();
        LOG_INFO << "END:" << g_strEndInfo;
        LOG_INFO << "g_iWriteCnt:" << g_iWriteCnt;
        LOG_INFO << "g_iReadCnt:" << g_iReadCnt;
        LOG_INFO << "g_iConnectResult:" << g_iConnectResult;
        LOG_INFO << "g_iReConnectResult:" << g_iReConnectResult;
        LOG_INFO << "g_iSendFullCnt:" << g_iSendFullCnt;
        LOG_INFO << "g_iSendLen:" << g_iSendLen << "->send_speed:" << send_speed;
        LOG_INFO << "g_iRecvLen:" << g_iRecvLen << "->recv_speed:" << recv_speed;
        LOG_INFO << "g_iConnNum:" << g_iConnNum;
        LOG_INFO << "get_connect_num:" << g_connect_num;
        LOG_INFO << "get_out_connect_num:" << g_out_connect_num;
        LOG_INFO << "get_in_connect_num:" << g_in_connect_num;
        LOG_INFO << "get_connection_num:" << g_connection_num;
        LOG_INFO << "g_client_error_num:" << g_client_error_num;
        LOG_INFO << "g_client_active_num:" << g_client_active_num;
        LOG_INFO << "g_client_fail_num:" << g_client_fail_num;

    }





}


class mock_tcp_socket_channel : public tcp_socket_channel
{
public:

    using ios_ptr = typename std::shared_ptr<io_service>;

    mock_tcp_socket_channel(const ios_ptr &ios, const socket_id &sid, const handler_create_functor &func,
                            int32_t len = 1024)
            : tcp_socket_channel(ios, sid, func, len) {}

    virtual int32_t start()
    {
        m_socket_handler = m_handler_functor(shared_from_this());
        return 0;
    }
};

class tcp_connector_mock : public tcp_connector
{
public:
    using ios_ptr = typename std::shared_ptr<io_service>;
    using nio_loop_ptr = typename std::shared_ptr<nio_loop_group>;

    typedef void (timer_handler_type)(const boost::system::error_code &);

    tcp_connector_mock(const nio_loop_ptr &connector_group, const nio_loop_ptr &worker_group,
                       const tcp::endpoint &connect_addr, const handler_create_functor &func)
            : tcp_connector(connector_group, worker_group, connect_addr, func) {}

    uint32_t send_msg(const std::string &strdata)
    {
        std::shared_ptr<message> msg = std::make_shared<message>();
        msg->set_name(strdata);
        assert(m_client_channel->is_stopped() == false);
        return m_client_channel->write(msg);
    }

    void test_reconnect()
    {
        reconnect("only test reconnect");
    }

    void stop_channel()
    {

        m_client_channel->stop();
    }
};


std::shared_ptr<tcp_connector_mock> g_testconnector = nullptr;


class mock_socket_channel_handler : public socket_channel_handler
{
public:
    virtual ~mock_socket_channel_handler() = default;

    mock_socket_channel_handler(std::shared_ptr<channel> ch)
            : _channel(ch) {}

    virtual int32_t start() { return 0; }

    virtual int32_t stop() { return 0; }

protected:
    std::weak_ptr<channel> _channel;
public:
    void send_msg(const string &m)
    {
        std::shared_ptr<message> msg = std::make_shared<message>();
        msg->set_name(m);
        auto ch = _channel.lock();
        assert(ch != nullptr);
        assert(ch->is_stopped() == false);
        ch->write(msg);
    }

    void stop_channel(channel_handler_context &ctx)
    {
        auto ch = _channel.lock();
        if (nullptr != ch)
        {
            ch->stop();
            CONNECTION_MANAGER->remove_channel(ch->id());
        }
    }

    virtual int32_t on_read(channel_handler_context &ctx, byte_buf &in)
    {
        auto step = TestNetwork::step();
        if (TestNetwork::CLI_CONNECT == step)
        {
            on_step_connect(ctx, in);
        }
        else if (TestNetwork::CLI_RECONNECT == step)
        {
            on_step_reconnect(ctx, in);
        }
        else if (TestNetwork::CLI_SEND_MORE_PACKATE == step || TestNetwork::CLI_RECV_MORE_PACKATE == step)
        {
            on_step_sendmore(ctx, in);
        }
        else if (TestNetwork::CLI_READ_WRITE_MULTI_PACKATE == step)
        {
            on_step_multipacket(ctx, in);
        }
        else if (TestNetwork::CLI_CONCURRENT_CONNECT ==step)
        {

        }
        else if (TestNetwork::CLI_SEND_NULL_DATA ==step)
        {
            on_step_sendnulldata(ctx, in);
        }
        else
       {
            in.move_read_ptr(in.get_valid_read_len());
            assert(1 == 0);
        }

        return 0;

    }

    virtual int32_t on_step_connect(channel_handler_context &ctx, byte_buf &in)
    {
        LOG_INFO << "[Test Connect OK !]";
        in.move_read_ptr(in.get_valid_read_len());
        TestNetwork::to_step(TestNetwork::CLI_SEND_MORE_PACKATE);

//        if (g_testconnector != nullptr)
//        {
// stop_channel(ctx);
//            g_testconnector->test_reconnect();
//        }
        return 0;
    }

    virtual int32_t on_step_reconnect(channel_handler_context &ctx, byte_buf &in)
    {
        LOG_INFO << "[Test Re-Connect OK !]";
        in.move_read_ptr(in.get_valid_read_len());
        TestNetwork::to_step(TestNetwork::CLI_SEND_MORE_PACKATE);
        return 0;
    }

    virtual int32_t on_step_sendnulldata(channel_handler_context &ctx, byte_buf &in)
    {
        LOG_INFO << " sendnull ";
        in.move_read_ptr(in.get_valid_read_len());
        send_msg("");
        TestNetwork::to_step(TestNetwork::CLI_END);
        return 0;
    }

    virtual int32_t on_step_sendmore(channel_handler_context &ctx, byte_buf &in)
    {
        if (TestNetwork::g_iRecvLen == 0)
        {
            TestNetwork::recv_start_time = high_resolution_clock::now();
        }

        TestNetwork::g_iRecvLen += in.get_valid_read_len();
        in.move_read_ptr(in.get_valid_read_len());
        if (TestNetwork::g_iSendEnd == 1 && TestNetwork::g_iRecvLen >= TestNetwork::g_iSendLen)
        {
            LOG_INFO << "[Recv OK],g_iRecvLen :" << TestNetwork::g_iRecvLen;
            TestNetwork::to_step(TestNetwork::CLI_READ_WRITE_MULTI_PACKATE);
            send_msg("hello");
        }
        return 0;

    }

    virtual int32_t on_step_multipacket(channel_handler_context &ctx, byte_buf &in)
    {
        if (TestNetwork::g_iWriteCnt == 0)
        {
            std::string strdata(in.get_read_ptr(), in.get_valid_read_len());
            in.move_read_ptr(in.get_valid_read_len());

            if (strdata != "hello")
            {
                LOG_INFO << " Write:" << TestNetwork::g_iWriteCnt << " ReadCnt:" << TestNetwork::g_iReadCnt
                          << " login failed on:" << strdata;
                TestNetwork::set_end("auth failed:" + strdata);
                return -1;
            }
            LOG_INFO << " Write:" << TestNetwork::g_iWriteCnt << " ReadCnt:" << TestNetwork::g_iReadCnt
                      << " login ok on:" << strdata;
            send_msg("");
            return 0;
        }

        uint32_t buf_len = 0;
        TestNetwork::get_string(TestNetwork::g_iWriteCnt, buf_len);
        if (in.get_valid_read_len() < buf_len)
        {
            LOG_INFO << " Write:" << TestNetwork::g_iWriteCnt << " ReadCnt:" << TestNetwork::g_iReadCnt
                      << " need more  on:";
            LOG_INFO << " buf_len:" << buf_len << " valid_read_len:" << in.get_valid_read_len() << ":"
                      << in.get_read_ptr();

            return -100;
        }

        bool isOK = TestNetwork::string_cmp(TestNetwork::g_iWriteCnt, (const char *) in.get_read_ptr(),
                                            in.get_valid_read_len());
        if (false == isOK)
        {
            LOG_INFO << "string_cmp failed Write:" << TestNetwork::g_iWriteCnt << " ReadCnt:" << TestNetwork::g_iReadCnt;
            TestNetwork::set_end("string_cmp failed:" + std::string(in.get_read_ptr()));
            in.move_read_ptr(in.get_valid_read_len());
            return -1;
        }

        TestNetwork::g_iReadCnt++;
        assert(TestNetwork::g_iWriteCnt == TestNetwork::g_iReadCnt);
        LOG_INFO << " Write:" << TestNetwork::g_iWriteCnt << " ReadCnt:" << TestNetwork::g_iReadCnt;
        in.move_read_ptr(in.get_valid_read_len());
        send_msg("");
        return 0;

    }

    virtual int32_t on_write(channel_handler_context &ctx, message &msg, byte_buf &buf)
    {
        do
        {
            const std::string &strdata = msg.get_name();
            if (!strdata.empty())
            {
                buf.write_to_byte_buf(strdata.data(), strdata.length());
                break;
            }

            auto step = TestNetwork::step();
            if (step == TestNetwork::CLI_READ_WRITE_MULTI_PACKATE)
            {
                if (TestNetwork::g_iWriteCnt >= 4)
                {
                    TestNetwork::set_end("__on_write__ successfully");
                    return -1;
                }
                TestNetwork::g_iWriteCnt++;
                uint32_t buf_len = 0;
                const char *in_buf = TestNetwork::get_string(TestNetwork::g_iWriteCnt, buf_len);
                buf.write_to_byte_buf(in_buf, buf_len);
                LOG_INFO << "on_step_multipacket on_write:"<<TestNetwork::g_iWriteCnt;
                break;
            }

            if (step == TestNetwork::CLI_SEND_MORE_PACKATE || step == TestNetwork::CLI_RECV_MORE_PACKATE)
            {
                if (TestNetwork::g_iSendLen == 0)
                {
                    TestNetwork::send_start_time = high_resolution_clock::now();
                }

                uint32_t buf_len = 0;
                const char *in_buf = TestNetwork::get_string(0, buf_len);
                buf.write_to_byte_buf(in_buf, buf_len);
                TestNetwork::g_iSendLen += buf_len;

                TestNetwork::g_iOnWriteCnt++;
                if(TestNetwork::g_iSendFullCnt != 0 && TestNetwork::g_iOnWriteCnt >=  TestNetwork::g_iSendFullCnt){
                    TestNetwork::g_iSendEnd = 1;
                }
                break;
            }

            if (step == TestNetwork::CLI_SEND_NULL_DATA)
            {
                LOG_INFO << " __on_write__: null ";
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                break;
            }

            uint32_t buf_len = 0;
            const char *in_buf = TestNetwork::get_string(0, buf_len);
            buf.write_to_byte_buf(in_buf, buf_len);
        } while (0);
        return 0;
    }

    virtual int32_t on_error() { return 0; }

public:

    virtual int32_t on_before_msg_send(message &msg) { return 0; }

    virtual int32_t on_after_msg_sent(message &msg)
    {

        return 0;
    }

    virtual int32_t on_before_msg_receive()
    {

        return 0;
    }

    virtual int32_t on_after_msg_received(message &msg)
    {

        return 0;
    }

    virtual bool is_logined() { return true; }

    static std::shared_ptr<socket_channel_handler> create(std::shared_ptr<channel> ch)
    {
        shared_ptr<socket_channel_handler> handler(new mock_socket_channel_handler(ch));
        return handler->shared_from_this();
    }

};


class TcpCli
{
public:
    using connector_ptr = typename std::shared_ptr<tcp_connector_mock>;

    TcpCli()
            :
            _worker_group(new nio_loop_group()),
            _connector_group(new nio_loop_group())
    {
        assert( NULL != TOPIC_MANAGER);
        TOPIC_MANAGER->unsubscribe<int32_t, std::shared_ptr<message> &>(TCP_CHANNEL_ERROR);
        TOPIC_MANAGER->unsubscribe<int32_t, std::shared_ptr<message> &>(CLIENT_CONNECT_NOTIFICATION);

        TOPIC_MANAGER->subscribe(TCP_CHANNEL_ERROR,
                                 [this](std::shared_ptr<message> &msg)
                                 {
                                     CONNECTION_MANAGER->remove_channel(msg->header.src_sid);
                                     TestNetwork::g_client_error_num++;
                                     return 0;
                                 }
        );


        TOPIC_MANAGER->subscribe(CLIENT_CONNECT_NOTIFICATION,
                                 [this](std::shared_ptr<message> &msg)
                                 {
                                     auto notification_content = std::dynamic_pointer_cast<client_tcp_connect_notification>(
                                             msg);

                                     TestNetwork::add_conn_num();
                                     if (CLIENT_CONNECT_SUCCESS == notification_content->status)
                                     {
                                         TestNetwork::g_client_active_num++;
                                     }
                                     else
                                     {
                                         TestNetwork::g_client_fail_num++;
                                     }
                                     return 0;
                                 }
        );
    }

    ~TcpCli()
    {

    }

    void start(int32_t connect_num)
    {

        _connector_group->init(1);
        _worker_group->init(1);

        _connector_group->start();
        _worker_group->start();

        tcp::endpoint _connect_addr(ip::address::from_string("127.0.0.1"), 11101);

        for (int i = 0; i < connect_num; ++i)
        {
            connector_ptr _connector;
            _connector = std::make_shared<tcp_connector_mock>(_connector_group, _worker_group, _connect_addr,
                                                              &mock_socket_channel_handler::create);

            if (i == 0)
            {
                g_testconnector = _connector;
            }

            _connector->start();
            _connector_list.push_back(_connector);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        }


    }

    void stop()
    {
        if (nullptr != g_testconnector)
        {
            g_testconnector = nullptr;
        }

        for (auto it = _connector_list.begin(); it != _connector_list.end(); ++it)
        {
            (*it)->stop();
        }

        _connector_list.clear();
        _connector_group->stop();
        _worker_group->stop();
        _connector_group->exit();
        _worker_group->exit();
    }


    std::shared_ptr<nio_loop_group> _worker_group;
    std::shared_ptr<nio_loop_group> _connector_group;
    std::vector<connector_ptr> _connector_list;
};


static void on_check_cli_task()
{
    time_t last_step_timestamp = TestNetwork::g_last_step_timestamp;
    TestNetwork::TEST_STEP last_step=TestNetwork::g_iTestStep;

    time_t now_time = time(NULL);
    time_t use_time = now_time - last_step_timestamp;
    LOG_INFO << std::string(TestNetwork::step_str(last_step)) << " use_time:" << use_time;


    if (TestNetwork::g_iTestEnd == 1)
    {
        LOG_INFO << "on_check_cli_task,test is finished:";
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        g_server->set_exited(true);
        return;
    }

    if (last_step == TestNetwork::CLI_SEND_MORE_PACKATE && TestNetwork::g_iSendEnd == 0)
    {
        for (int j = 0; j < MAX_SEND_QUEUE_MSG_COUNT * 2; ++j)//
        {
            if (g_testconnector != nullptr)
            {
                if (0 != g_testconnector->send_msg(""))
                {
                    TestNetwork::g_iSendFullCnt = j;
                    LOG_INFO << "[Send Overflow],sendcnt :" << j;
                    break;
                }

            }
        }
        TestNetwork::to_step(TestNetwork::CLI_RECV_MORE_PACKATE);

        return;
    }


    if (TestNetwork::CLI_READ_WRITE_MULTI_PACKATE == last_step && use_time > 10)
    {
        TestNetwork::set_end("on_check_cli_task CLI_READ_WRITE_MULTI_PACKATE timeout");
        g_server->set_exited(true);
        return;
    }

    if (TestNetwork::CLI_RECV_MORE_PACKATE == last_step&& use_time > 600)
    {
        TestNetwork::set_end("on_check_cli_task CLI_RECV_MORE_PACKATE timeout");
        g_server->set_exited(true);
        return;
    }

    if (TestNetwork::CLI_END ==last_step && use_time > 5)
    {
        TestNetwork::set_end("cli_end_timeout");
        g_testconnector->stop_channel();
        g_server->set_exited(true);
        return;
    }


    if (TestNetwork::CLI_CONCURRENT_CONNECT == last_step && use_time > 5)
    {
        connection_manager_mock *cm = (connection_manager_mock *) g_server->get_connection_manager();
        TestNetwork::g_connect_num = cm->get_connect_num();
        TestNetwork::g_out_connect_num = cm->get_out_connect_num();
        TestNetwork::g_in_connect_num = cm->get_in_connect_num();

        LOG_INFO << "TestNetwork::g_out_connect_num:" << TestNetwork::g_out_connect_num << " conn:"
                  << TestNetwork::g_iConnNum;

        if ( int(TestNetwork::g_out_connect_num) >= int(TestNetwork::get_valid_conn_num()) &&
             int(TestNetwork::g_iConnNum) >= int(TestNetwork::get_valid_conn_num()) )
        {
            TestNetwork::set_end("CLI_CONCURRENT_CONNECT");
            g_server->set_exited(true);
            return;
        }
    }

}


static void on_check_task()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    auto step = TestNetwork::step();
    if (step >= TestNetwork::CLI_BEGIN && step <= TestNetwork::CLI_END)
    {
        on_check_cli_task();
        return;
    }


}


class mock_acceptor_channel_handler : public mock_socket_channel_handler
{
public:
    mock_acceptor_channel_handler(std::shared_ptr<channel> ch)
            : mock_socket_channel_handler(ch)
    {

    }

public:
    virtual int32_t on_read(channel_handler_context &ctx, byte_buf &in)
    {
        int32_t read_len = in.get_valid_read_len();
        std::string strdata(in.get_read_ptr(), read_len);
        in.move_read_ptr(read_len);
        send_msg(strdata);
        return 0;
    }

    virtual int32_t on_error() { return 0; }


    virtual int32_t on_before_msg_receive()
    {
        send_msg("hello");
        return 0;
    }

public:
    static std::shared_ptr<socket_channel_handler> create(std::shared_ptr<channel> ch)
    {
        shared_ptr<socket_channel_handler> handler(new mock_acceptor_channel_handler(ch));
        return handler->shared_from_this();
    }
};

class TcpSvr
{
public:
    TcpSvr()
            : _worker_group(new nio_loop_group()),
              _acceptor_group(new nio_loop_group())
    {
    }

    ~TcpSvr()
    {
    }

    void start()
    {
        _acceptor_group->init(1);
        _worker_group->init(1);
        _acceptor_group->start();
        _worker_group->start();

        std::weak_ptr<io_service> ios(_acceptor_group->get_io_service());
        tcp::endpoint svr_ep(tcp::v4(), 11101);//11101
        _acceptor = std::make_shared<tcp_acceptor>(ios, _worker_group, svr_ep, &mock_acceptor_channel_handler::create);
         _acceptor->start();
    }

    void stop()
    {
        _acceptor->stop();
        _acceptor_group->stop();
        _worker_group->stop();

        _acceptor_group->exit();
        _worker_group->exit();
    }

    std::shared_ptr<nio_loop_group> _worker_group;
    std::shared_ptr<nio_loop_group> _acceptor_group;
    std::shared_ptr<tcp_acceptor> _acceptor;
};

void test_cli_sendrecv()
{
    TestNetwork::g_iTestEnd = 0;
    TestNetwork::to_step(TestNetwork::CLI_CONNECT);
    TcpCli cli;
    cli.start(1);
    g_server->idle();
    cli.stop();
}

void test_cli_cnn120()
{
    TestNetwork::test_conn_max = 120;
    TestNetwork::g_iTestEnd = 0;
    TestNetwork::to_step(TestNetwork::CLI_CONCURRENT_CONNECT);
    TcpCli cli;
    cli.start(TestNetwork::test_conn_max);
    g_server->idle();
    TestNetwork::to_step(TestNetwork::CLI_END);
    connection_manager_mock *cm = (connection_manager_mock *) g_server->get_connection_manager();
    TestNetwork::g_connect_num = cm->get_connect_num();
    TestNetwork::g_out_connect_num = cm->get_out_connect_num();
    TestNetwork::g_in_connect_num = cm->get_in_connect_num();
    TestNetwork::g_connection_num = cm->get_connection_num();
    cli.stop();
}


void test_cli_cnn128()
{
    TestNetwork::test_conn_max = 128;
    TestNetwork::g_iTestEnd = 0;
    TestNetwork::to_step(TestNetwork::CLI_CONCURRENT_CONNECT);
    TcpCli cli;
    cli.start(TestNetwork::test_conn_max);
    g_server->idle();
    TestNetwork::to_step(TestNetwork::CLI_END);
    connection_manager_mock *cm = (connection_manager_mock *) g_server->get_connection_manager();
    TestNetwork::g_connect_num = cm->get_connect_num();
    TestNetwork::g_out_connect_num = cm->get_out_connect_num();
    TestNetwork::g_in_connect_num = cm->get_in_connect_num();
    TestNetwork::g_connection_num = cm->get_connection_num();
    cli.stop();
}

//
//void test_cli3()
//{
//    TestNetwork::g_iTestEnd = 0;
//    TestNetwork::to_step(TestNetwork::CLI_SEND_NULL_DATA);
//    TestNetwork::g_iConnNum = 0;
//    TcpCli cli;
//    cli.start(1);
//    g_server->idle();
//    cli.stop();
//}
//

class TestEnv
{
public:
    typedef void(*FUNC)(void);
    TestEnv(FUNC func)
    {
        matrix::core::log::init();
        TestNetwork::begin();
        server_initiator_factory::bind_creator(create_functor_type(dbc_server_initiator_mock::create_initiator));
        g_server->set_exited(false);
        g_server->bind_idle_task(&on_check_task);
        g_server->init(0, NULL);

        m_tcpsvr= std::make_shared<TcpSvr>();
        m_tcpsvr->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        if(NULL != func)func();

    }
    ~TestEnv()
    {
        m_tcpsvr->stop();
        m_tcpsvr= nullptr;
        g_server->exit();
        TestNetwork::end();

    }
    std::shared_ptr<TcpSvr> m_tcpsvr;
};




BOOST_AUTO_TEST_CASE(test_connection_manager_network_connect_disconnect_128)
{
    TestEnv env(test_cli_cnn128) ;
    std::string path=TestNetwork::path_str();
    std::string path2 = "->CLI_CONCURRENT_CONNECT->CLI_END";
    BOOST_TEST_CHECK(path == path2, path);
    BOOST_TEST_CHECK(TestNetwork::g_in_connect_num <= TestNetwork::conn_max, "test get_in_connect_num()");
    BOOST_TEST_CHECK(TestNetwork::g_in_connect_num + TestNetwork::g_out_connect_num == TestNetwork::g_connect_num,
                     "test get_connect_num()");
    BOOST_TEST_CHECK(TestNetwork::g_client_active_num == TestNetwork::test_conn_max, "test connect()");
    BOOST_TEST_CHECK(TestNetwork::g_in_connect_num == TestNetwork::g_out_connect_num, "test connect()");
}

BOOST_AUTO_TEST_CASE(test_connection_manager_network_connect_disconnect_120)
{
    TestEnv env(test_cli_cnn120) ;
    std::string path=TestNetwork::path_str();
    std::string path2 = "->CLI_CONCURRENT_CONNECT->CLI_END";
    BOOST_TEST_CHECK(path == path2, path);

    BOOST_TEST_CHECK(TestNetwork::g_in_connect_num <= TestNetwork::conn_max, "test get_in_connect_num()");
    BOOST_TEST_CHECK(TestNetwork::g_in_connect_num + TestNetwork::g_out_connect_num == TestNetwork::g_connect_num,
                     "test get_connect_num()");

    BOOST_TEST_CHECK(TestNetwork::g_client_active_num == TestNetwork::test_conn_max, "test connect()");
    BOOST_TEST_CHECK(TestNetwork::g_in_connect_num == TestNetwork::g_out_connect_num, "test connect()");
}


BOOST_AUTO_TEST_CASE(test_connection_manager)
{
    int32_t ret;
    server_initiator_factory::bind_creator(create_functor_type(dbc_server_initiator_mock::create_initiator));
    g_server->init(0, NULL);

    socket_id _socket_id(SERVER_SOCKET, 1);
    auto context = std::make_shared<boost::asio::io_context>();
    auto _tcp_socket_channel = std::make_shared<mock_tcp_socket_channel>(context, _socket_id,
                                                                         &mock_socket_channel_handler::create);
    _tcp_socket_channel->set_remote_node_id("2gfpp3MAB3zWhesVATX5Y4Ttd2wXf3caoFnqzEmZeet");
    _tcp_socket_channel->start();

    ret = CONNECTION_MANAGER->add_channel(_socket_id, _tcp_socket_channel);
    BOOST_TEST_CHECK(E_SUCCESS == ret, "CONNECTION_MANAGER->add_channel() normal");

    if (E_SUCCESS == ret)
    {
        BOOST_TEST_CHECK(CONNECTION_MANAGER->get_connection_num() == 1, " CONNECTION_MANAGER->get_connection_num() ");

        ret = CONNECTION_MANAGER->add_channel(socket_id(SERVER_SOCKET, 1), _tcp_socket_channel);
        BOOST_TEST_CHECK(E_SUCCESS != ret, "CONNECTION_MANAGER->add_channel() abnormal");

        vector<string> path;
        path.push_back("2gfpp3MAB4Jo9SaiLrg68Po5WvJKt1rSjzsT9uSpxL3");
        path.push_back("2gfpp3MAB4ACimTtKJtNUj2cGzaZhcwcg3NuNUmgZTT");
        path.push_back("2gfpp3MAB3zWhesVATX5Y4Ttd2wXf3caoFnqzEmZeet");
        auto _channel = CONNECTION_MANAGER->find_fast_path(path);
        BOOST_TEST_REQUIRE(_channel != nullptr);
        BOOST_TEST(path.size() == 2);

        auto _tcp_socket_channel2 = CONNECTION_MANAGER->get_channel(_socket_id);
        BOOST_TEST_CHECK(_tcp_socket_channel2 == _tcp_socket_channel, " CONNECTION_MANAGER->get_channel()");

        CONNECTION_MANAGER->remove_channel(_socket_id);
        auto _tcp_socket_channel3 = CONNECTION_MANAGER->get_channel(_socket_id);
        BOOST_TEST_CHECK(_tcp_socket_channel3 == nullptr, " CONNECTION_MANAGER->remove_channel()");
    }


    int port = 1999;
    tcp::endpoint svr_ep(tcp::v4(), port);
    ret = CONNECTION_MANAGER->start_listen(svr_ep, &mock_socket_channel_handler::create);
    BOOST_TEST_CHECK(E_SUCCESS == ret, " _connection_manager.start_listen");
//    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    tcp::endpoint cli_ep(ip::address::from_string("127.0.0.1"), port);
    ret = CONNECTION_MANAGER->start_connect(cli_ep, &mock_socket_channel_handler::create);
    BOOST_TEST_CHECK(E_SUCCESS == ret, "_connection_manager.start_connect");
//    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    CONNECTION_MANAGER->stop_connect(cli_ep);
    CONNECTION_MANAGER->stop_listen(svr_ep);
    g_server->exit();

}


BOOST_AUTO_TEST_CASE(test_find_fast_path_no_match_path)
{
    connection_manager cm;
    bpo::variables_map v;
    auto context = std::make_shared<boost::asio::io_context>();
    auto ch = std::make_shared<mock_tcp_socket_channel>(context, socket_id(SERVER_SOCKET, 1),
                                                        &mock_socket_channel_handler::create);
    ch->start();

    ch->set_remote_node_id("2gfpp3MAB4ACimTtKJtNUj2cGzaZhcwcg3NuNUmgZHH");
    cm.add_channel(socket_id(SERVER_SOCKET, 1), ch);

    vector<string> path;
    path.push_back("2gfpp3MAB4Jo9SaiLrg68Po5WvJKt1rSjzsT9uSpxL3");
    path.push_back("2gfpp3MAB4ACimTtKJtNUj2cGzaZhcwcg3NuNUmgZTT");
    path.push_back("2gfpp3MAB3zWhesVATX5Y4Ttd2wXf3caoFnqzEmZeet");
    auto ch1 = cm.find_fast_path(path);
    BOOST_TEST_REQUIRE(ch1 == nullptr);
    BOOST_TEST(path.size() == 3);
}

BOOST_AUTO_TEST_CASE(test_find_fast_path_no_live_connection)
{
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

BOOST_AUTO_TEST_CASE(test_connection_manager_network_send_recv)
{
    TestEnv env(test_cli_sendrecv) ;
    std::string path=TestNetwork::path_str();
    std::string path2 = "->CLI_CONNECT->CLI_SEND_MORE_PACKATE->CLI_RECV_MORE_PACKATE->CLI_READ_WRITE_MULTI_PACKATE";
    BOOST_TEST_CHECK(path == path2, path);
    BOOST_TEST_CHECK(TestNetwork::g_iWriteCnt == 4, "test write");
    BOOST_TEST_REQUIRE(TestNetwork::g_iReadCnt == 3, "test read");
    BOOST_TEST_REQUIRE(TestNetwork::g_iSendFullCnt >=MAX_SEND_QUEUE_MSG_COUNT, "test send full");
    BOOST_TEST_CHECK(TestNetwork::g_iSendLen == TestNetwork::g_iRecvLen, "test write read more ");
}