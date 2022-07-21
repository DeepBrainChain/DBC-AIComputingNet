// references: https://www.boost.org/doc/libs/1_76_0/doc/html/boost_asio/example/cpp11/timeouts/async_tcp_client.cpp

#include "zabbixSender.h"
#include <iostream>
#include <functional>
#include <string.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#define ZBX_TCP_HEADER_DATA "ZBXD"

zabbixSender::zabbixSender(boost::asio::io_service& io_service,
        const std::string& host, const std::string& port)
    : socket_(io_service)
    , resolver_(io_service)
    , deadline_(io_service)
    , queue_timer_(io_service)
    , server_({host, port}) {
    // msg_queue_.swap(msg_queue);
    // tcp::resolver r(io_service);
    input_buffer_.resize(max_length_);
}

bool zabbixSender::start() {
    if (msg_queue_.empty()) return false;
    stopped_ = false;
    endpoints_ = resolver_.resolve(server_.host, server_.port);
    start_connect(endpoints_.begin());

    deadline_.async_wait(std::bind(&zabbixSender::check_deadline, this));
}

bool zabbixSender::stop() {
    stopped_ = true;
    boost::system::error_code ignored_error;
    socket_.close(ignored_error);
    deadline_.cancel();
    queue_timer_.cancel();
    std::cout << "zabbix sender closed" << std::endl;

    queue_timer_.expires_after(std::chrono::seconds(1));
    queue_timer_.async_wait(std::bind(&zabbixSender::start, this));
}

void zabbixSender::push(const std::string& hostname, const std::string& message) {
    msg_queue_.push(std::make_pair(hostname, message));
}

void zabbixSender::start_connect(tcp::resolver::results_type::iterator endpoint_iter) {
    if (endpoint_iter != endpoints_.end()) {
        std::cout << "Trying " << endpoint_iter->endpoint() << "...\n";

        deadline_.expires_after(std::chrono::seconds(10));

        socket_.async_connect(endpoint_iter->endpoint(),
            std::bind(&zabbixSender::handle_connect, this, _1, endpoint_iter));
    } else {
        stop();
    }
}

void zabbixSender::handle_connect(const boost::system::error_code& error,
    tcp::resolver::results_type::iterator endpoint_iter) {
    if (stopped_) return;

    if (!socket_.is_open()) {
        std::cout << "Connect timed out" << std::endl;
        start_connect(++endpoint_iter);
    } else if (error) {
        std::cout << "Connect error: " << error.message() << std::endl;
        socket_.close();
        start_connect(++endpoint_iter);
    } else {
        std::cout << "Connected to " << endpoint_iter->endpoint() << std::endl;
        socket_.non_blocking(true);
        socket_.set_option(tcp::no_delay(true));
        socket_.set_option(boost::asio::socket_base::keep_alive(true));
        process_queue();
    }
}

void zabbixSender::process_queue() {
    if (msg_queue_.empty()) {
        stop();
        return;
    }

    std::pair<std::string, std::string> data = msg_queue_.front();
    // wants_[data.first] = true;
    auto iter = wants_.find(data.first);
    if (iter != wants_.end()) {
        msg_queue_.pop();
        if (iter->second) {
            send_json_data(data.second);
            start_read(0);
        } else {
            process_queue();
        }
    } else {
        is_server_want_monitor_data(data.first);
        start_read(1);
    }
}

void zabbixSender::send_json_data(const std::string &json_data) {
    std::string data = "ZBXD\x01";
    unsigned long long data_len = json_data.length();
    char* arrLen = reinterpret_cast<char*>(&data_len);
    data.append(arrLen, 8);
    data.append(json_data);
    start_write(data);
}

void zabbixSender::is_server_want_monitor_data(const std::string& hostname) {
    rapidjson::StringBuffer strBuf;
    rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
    write.StartObject();
    write.Key("request");
    write.String("active checks");
    write.Key("host");
    write.String(hostname.c_str());
    write.EndObject();
    std::string json_data = strBuf.GetString();
    send_json_data(json_data);
}

void zabbixSender::start_write(const std::string &data) {
    if (stopped_) return;

    boost::asio::async_write(socket_, boost::asio::buffer(data),
        std::bind(&zabbixSender::handle_write, this, _1));
}

void zabbixSender::handle_write(const boost::system::error_code& error) {
    if (stopped_) return ;
    
    if (!error) {
        std::cout << "send data success" << std::endl;
        // queue_timer_.expires_after(std::chrono::seconds(10));
        // queue_timer_.async_wait(std::bind(&zabbixSender::process_queue, this));
    } else {
        std::cout << "Error on send data: " << error.message() << std::endl;
        stop();
    }
}

// type == 0 send monitor data | type == 1 query server wants
void zabbixSender::start_read(int type) {
    deadline_.expires_after(std::chrono::seconds(30));

    socket_.async_read_some(boost::asio::buffer(input_buffer_),
        std::bind(&zabbixSender::handle_read, this, _1, _2, type));

    // socket_.async_read_some(boost::asio::buffer(input_buffer_),
    //     [this, type](boost::system::error_code ec, std::size_t n) {
    //         if (stopped_) return;

    //         if (!ec) {
    //             std::string response(input_buffer_.substr(0, n));
    //             input_buffer_.erase(0, n);

    //             if (!response.empty()) {
    //                 std::cout << "Received: " << response << std::endl;
    //                 do_read(response, type);
    //             }

    //             process_queue();
    //             // stop();
    //         } else {
    //             std::cout << "Error on receive: " << ec.message() << std::endl;
    //             stop();
    //         }
    //     }
    //     );
}

void zabbixSender::handle_read(const boost::system::error_code& error, std::size_t n, int type) {
    if (stopped_) return;

    if (!error) {
        std::string response(input_buffer_.substr(0, n));
        input_buffer_.erase(0, n);

        if (!response.empty()) {
            std::cout << "Received: " << response << std::endl;
            do_read(response, type);
        }

        stop();
        // boost::system::error_code ignored_error;
        // socket_.shutdown(tcp::socket::shutdown_both, ignored_error);
        // socket_.close(ignored_error);

        // queue_timer_.expires_after(std::chrono::seconds(1));
        // queue_timer_.async_wait(std::bind(&zabbixSender::start, this));
    } else {
        std::cout << "Error on receive: " << error.message() << std::endl;
        stop();
    }
}

void zabbixSender::do_read(const std::string& response, int type) {
    unsigned long long jsonLength = 0;
    const char* jsonstr = get_json_string(response.c_str(), jsonLength);
    if (jsonstr && jsonLength > 0 && response.length() > jsonLength) {
        bool check = check_response(jsonstr);
        if (type == 1) {
            std::pair<std::string, std::string> data = msg_queue_.front();
            wants_[data.first] = check;
            if (!check)
                std::cout << "server does not want host(" << data.first << ")" << std::endl;
        }
        if (check) {
            //
        }
    } else {
        // std::cout << "parse zabbix reply error" << std::endl;
        std::cout << "parse zabbix reply error(string begin: "
            << jsonstr << ", string length: "
            << jsonLength << ")" << std::endl;
    }
}

bool zabbixSender::check_response(const char* response) {
    if (!response) return false;
    
    rapidjson::Document doc;
    doc.Parse(response);
    if (!doc.IsObject()) {
        std::cout << "parse response json failed" << std::endl;
        return false;
    }
    if (!doc.HasMember("response")) {
        std::cout << "can not find response node" << std::endl;
        return false;
    }
    if (!doc["response"].IsString()) {
        std::cout << "response node is not a string node" << std::endl;
        return false;
    }
    std::string result = doc["response"].GetString();
    return result == "success";
}

const char* zabbixSender::get_json_string(const char* result, unsigned long long &jsonLength) {
    const char* zbx_tcp_header = strstr(result, ZBX_TCP_HEADER_DATA);
    if (!zbx_tcp_header) return nullptr;
    jsonLength = *reinterpret_cast<const unsigned long long*>(zbx_tcp_header + 5);
    return zbx_tcp_header + 5 + 8;
}

void zabbixSender::check_deadline() {
    if (stopped_) return;

    if (deadline_.expiry() <= steady_timer::clock_type::now()) {
        std::cout << "deadline expired" << std::endl;

        socket_.close();

        deadline_.expires_at(steady_timer::time_point::max());
    }

    deadline_.async_wait(std::bind(&zabbixSender::check_deadline, this));
}
