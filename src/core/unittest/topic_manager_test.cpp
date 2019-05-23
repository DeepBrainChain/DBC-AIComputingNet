/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : topic_manager_test.cpp
* description       :
* date              : 2018/8/30
* author            : Taz Zhang
**********************************************************************************/
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

#include "topic_manager.h"
#include <string>
#include <boost/asio.hpp>
#include <queue>
#include <mutex>
#include "function_traits.h"
#include <chrono>
#include <vector>

using namespace std::chrono;
using namespace std;
using namespace boost::unit_test;

class service_module_mock
{
public:
    enum CONSTANT
    {
        MAX_MSG_COUNT = 10,
        WORKER_COUNT = 8,
    };
    struct MSG
    {
        uint32_t num;
        std::mutex lock;
    };


    typedef std::function<void(void *)> task_functor;
    typedef std::queue<std::shared_ptr<MSG>> queue_type;

    static void module_task(void *argv)
    {
        service_module_mock *module = (service_module_mock *) argv;
        module->run();
    }

    service_module_mock(uint32_t _module_id, std::shared_ptr<matrix::core::topic_manager> _topic_manager)
            : m_exited(false),
              m_module_task(module_task),
              m_module_id(_module_id),
              m_topic_manager(_topic_manager)
    {
    }

    ~service_module_mock()
    {
    }

    int32_t start()
    {
        m_thread = std::make_shared<std::thread>(m_module_task, this);
        init_subscription();
        return E_SUCCESS;
    }

    int32_t stop()
    {
        m_exited = true;
        if (m_thread != nullptr)
        {
            try
            {
                m_thread->join();
            }
            catch (const std::system_error &e)
            {
                LOG_ERROR << "join thread(" << m_thread->get_id() << ")error: " << e.what();
                return E_DEFAULT;
            }
            catch (...)
            {
                LOG_ERROR << "join thread(" << m_thread->get_id() << ")error: unknown";
                return E_DEFAULT;
            }
        }
        return E_SUCCESS;
    }


    int32_t run()
    {

        queue_type tmp_msg_queue;
        while (!m_exited)
        {
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                std::chrono::milliseconds ms(500);
                m_cond.wait_for(lock, ms, [this]() -> bool { return m_msg_queue.size() > 0; });
                m_msg_queue.swap(tmp_msg_queue);
            }
            std::shared_ptr<MSG> msg;
            while (!tmp_msg_queue.empty())
            {
                msg = tmp_msg_queue.front();
                tmp_msg_queue.pop();
                on_invoke(msg);
            }
        }

        return E_SUCCESS;
    }

    int32_t send(std::shared_ptr<MSG> msg)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_msg_queue.size() < MAX_MSG_COUNT)
        {
            m_msg_queue.push(msg);          //right value ref
        }
        else
        {
            LOG_ERROR << "service module message queue full";
            return E_MSG_QUEUE_FULL;
        }

        if (!m_msg_queue.empty())
        {
            m_cond.notify_all();
        }
        return E_SUCCESS;
    }

    void init_subscription()
    {
        m_topic_manager->subscribe("add", [this](std::shared_ptr<MSG> &msg) { return send(msg); });
    }

    void del_subscription()
    {
        m_topic_manager->unsubscribe<int32_t>("add");
    }

    void del_subscription2()
    {
        m_topic_manager->unsubscribe<int32_t, std::shared_ptr<MSG> &>("add");
    }

    int32_t on_invoke(std::shared_ptr<MSG> &msg)
    {
        std::unique_lock<std::mutex> lock(msg->lock);
        msg->num += 1;
        return 0;
//        LOG_INFO << "on_invoke module:" << m_module_id << " get num:" << msg->num;
    }


protected:
    bool m_exited;
    mutex m_mutex;
    std::shared_ptr<std::thread> m_thread;
    queue_type m_msg_queue;
    condition_variable m_cond;
    task_functor m_module_task;
    uint32_t m_module_id;
    std::shared_ptr<matrix::core::topic_manager> m_topic_manager;

};



BOOST_AUTO_TEST_CASE(service_bus_test) {

    matrix::core::topic_manager tm;


    std::string login_out1;
    tm.subscribe("login", [&](std::string _name) { login_out1=_name; }     );

    std::string login_out2;
    tm.subscribe("login", [&](std::string _name,std::string _hello) { login_out2=_name+_hello; }     );

    std::string login_out3;
    tm.subscribe("login", [&](std::string _name,std::string _hello,std::string _time) { login_out3=_name+_hello+_time; }     );

    tm.publish<void, std::string>("sport", "soccer");


    tm.publish<void, std::string>("login", "Jim");
    BOOST_TEST_CHECK(login_out1=="Jim","publish2");

    tm.publish<void, std::string,std::string>("login", "Jim","welcome");
    BOOST_TEST_CHECK(login_out2=="Jimwelcome","publish3");

    tm.publish<void, std::string,std::string,std::string>("login", "Jim","welcome","2010237377");
    BOOST_TEST_CHECK(login_out3 == "Jimwelcome2010237377","publish4");


    std::string say_out1;
    std::string say_out2;
    std::string say_out3;

    tm.subscribe("say", [&](std::string _msg) { say_out1="Jim"+_msg; }     );
    tm.subscribe("say", [&](std::string _msg) { say_out2="Adm"+_msg; }     );
    tm.subscribe("say", [&](std::string _msg) { say_out3="Boo"+_msg; }     );
    tm.publish<void, std::string>("say", "12345678");

    BOOST_TEST_CHECK(say_out1=="Jim12345678");
    BOOST_TEST_CHECK(say_out2=="Adm");
    BOOST_TEST_CHECK(say_out3=="Boo");


    tm.subscribe("say2", [&]( std::string& _msg) { say_out1="Jim"+_msg; }     );
    tm.subscribe("say2", [&]( std::string& _msg) { say_out2="Adm"+_msg; }     );
    tm.subscribe("say2", [&]( std::string& _msg) { say_out3="Boo"+_msg; }     );

    std::string ss( "12345678" );
    tm.publish<void, std::string& >("say2", ss);

    BOOST_TEST_CHECK(say_out1=="Jim12345678");
    BOOST_TEST_CHECK(say_out2=="Adm12345678");
    BOOST_TEST_CHECK(say_out3=="Boo12345678");

}

BOOST_AUTO_TEST_CASE(service_bus_work_in_thread)
{
    std::shared_ptr<matrix::core::topic_manager> _topic_manager = std::make_shared<matrix::core::topic_manager>();


    using module_ptr = typename std::shared_ptr<service_module_mock>;
    std::vector<module_ptr> modules;
    for (int i = 0; i < service_module_mock::WORKER_COUNT; ++i)
    {
        module_ptr _module = std::make_shared<service_module_mock>(i, _topic_manager);

        _module->start();

//        if (i == service_module_mock::WORKER_COUNT - 1)
//        {
//            _module->del_subscription2();
//        }

        modules.push_back(_module);
    }
    uint32_t begin = 1;

    std::shared_ptr<service_module_mock::MSG> msg = std::make_shared<service_module_mock::MSG>();
    msg->num = begin;
    _topic_manager->publish<int32_t>("add", msg);

    for (int i = 0; i < service_module_mock::WORKER_COUNT; ++i)
    {
        modules[i]->stop();
    }
    modules.clear();
    BOOST_TEST_CHECK(msg->num == begin + service_module_mock::WORKER_COUNT, "test multi-subscribe");
}

