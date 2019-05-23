// The following two lines indicates boost test with Shared Library mode
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

#include "os_math.h"

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <iostream>
#include <boost/asio/connect.hpp>
#include <boost/asio/steady_timer.hpp>

using namespace boost::asio;

#include <thread>
#include <boost/bind.hpp>
#include <boost/asio/io_service.hpp>

void dummy_work(){
    int i=0;
    std::cin>>i;
}



BOOST_AUTO_TEST_CASE(test_io_context) {
    io_context *c = new io_context;
    std::shared_ptr <io_context> cp (c);

//    std::cout<<"a) " <<cp.use_count()<<std::endl;

    //deprecate io_service
//    io_service::work w{ioservice};  //add an outstanding work (fake), then io_service::run() will not exit

    auto work = boost::asio::make_work_guard(*cp);

    std::thread thread1 {boost::bind(&io_context::run,cp)};

//    std::cout<<"b) " << cp.use_count()<<std::endl;


    typedef void (timer_handler_type)(const boost::system::error_code &);
    std::function<timer_handler_type> h;
    h = [cp,&work](const boost::system::error_code &ec){
        std::cout<<"c) " << cp.use_count()<<std::endl;
        std::cout<<"1 sec\n";
        work.reset();
//        c.stop();
    };



    steady_timer timer1{*cp, std::chrono::seconds{1}};
    timer1.expires_from_now(std::chrono::seconds(1));
    timer1.async_wait(h);

//    std::cout<<"d) " << cp.use_count()<<std::endl;


    thread1.join();
//    std::cout<<"e) " << cp.use_count()<<std::endl;


    h = nullptr;

//    std::cout<<"f) " << cp.use_count()<<std::endl;

}