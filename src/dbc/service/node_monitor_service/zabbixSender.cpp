#include "zabbixSender.h"
#include <boost/bind.hpp>
#include "log/log.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

using boost::asio::steady_timer;
using boost::asio::ip::tcp;

#define ZBX_TCP_HEADER_DATA "ZBXD"

///////////////////////////////////////////////////////////////////////////////

zabbixResponse::zabbixResponse() : body_length_(0) {
    data_.resize(header_length + max_body_length);
}

const char* zabbixResponse::data() const {
    return data_.c_str();
}

char* zabbixResponse::data() {
    return &data_[0];
}

std::size_t zabbixResponse::length() const {
    return header_length + body_length_;
}

const char* zabbixResponse::body() const {
    return data_.c_str() + header_length;
}

char* zabbixResponse::body() {
    return &data_[header_length];
}

std::size_t zabbixResponse::body_length() const {
    return body_length_;
}

bool zabbixResponse::decode_header() {
    body_length_ = 0;
    if (data_.find(ZBX_TCP_HEADER_DATA) == std::string::npos)
        return false;
    char header[header_length + 1] = {0};
    std::strncat(header, data_.c_str(), header_length);
    unsigned long long jsonLength =
        *reinterpret_cast<unsigned long long*>(header + 5);
    if (jsonLength > max_body_length) return false;
    if (jsonLength <= 0) return false;
    body_length_ = jsonLength;
    return true;
}

void zabbixResponse::clear() {
    data_.erase(0, header_length + body_length_);
    body_length_ = 0;
}

///////////////////////////////////////////////////////////////////////////////

zabbixSender::zabbixSender(boost::asio::io_service& io_service,
    const std::string& host, const std::string& port)
    : resolver_(io_service)
    , socket_(io_service)
    , deadline_(io_service)
    , server_({host, port})
    , queue_timer_(io_service) {

}

void zabbixSender::start() {
    queue_timer_.async_wait(boost::bind(&zabbixSender::queue_process, shared_from_this()));
}

void zabbixSender::stop_graceful() {
    stopped_ = true;
    boost::system::error_code ignored_error;
    // socket_.cancel(ignored_error);
    socket_.close(ignored_error);
    deadline_.cancel(ignored_error);
    LOG_INFO << "zabbix sender stopped graceful";

    std::queue<std::pair<std::string, std::string>> empty_queue;
    wait_queue_.swap(empty_queue);
    queue_timer_.cancel(ignored_error);
}

void zabbixSender::stop() {
    stopped_ = true;
    boost::system::error_code ignored_error;
    // socket_.cancel(ignored_error);
    socket_.close(ignored_error);
    deadline_.cancel(ignored_error);
    LOG_INFO << "zabbix sender stopped";

    queue_timer_.expires_after(std::chrono::seconds(10));
    queue_timer_.async_wait(boost::bind(&zabbixSender::queue_process, shared_from_this()));
}

void zabbixSender::push(const std::string& hostname, const std::string& json) {
    auto iter = wants_.find(hostname);
    if (iter != wants_.end()) {
        int64_t cur_time = time(NULL);
        if (difftime(cur_time, iter->second) >= 600) {
            LOG_INFO << "server " << server_.host << " does not need " << hostname;
            return;
        }
    }

    if (wait_queue_.size() > 100) {
        LOG_INFO << "msg queue of server " << server_.host << " is full";
        // return;
        wait_queue_.pop();
    }
    wait_queue_.push(std::make_pair(hostname, json));
}

void zabbixSender::queue_process() {
    if (msg_queue_.empty() && !wait_queue_.empty()) {
        auto iter = wait_queue_.front();
        wait_queue_.pop();
        msg_queue_.push(std::move(iter));
    }
    
    if (msg_queue_.empty()) {
        queue_timer_.expires_after(std::chrono::seconds(10));
        queue_timer_.async_wait(boost::bind(&zabbixSender::queue_process, shared_from_this()));
        return;
    }

    stopped_ = false;

    endpoints_ = resolver_.resolve(server_.host, server_.port);
    start_connect(endpoints_.begin());

    deadline_.async_wait(boost::bind(&zabbixSender::check_deadline, shared_from_this()));
}

void zabbixSender::start_connect(tcp::resolver::results_type::iterator endpoint_iter) {
    if (endpoint_iter != endpoints_.end()) {
        LOG_INFO << "Trying " << endpoint_iter->endpoint() << "...";

        deadline_.expires_after(std::chrono::seconds(10));

        socket_.async_connect(endpoint_iter->endpoint(),
            boost::bind(&zabbixSender::handle_connect, shared_from_this(),
                boost::asio::placeholders::error, endpoint_iter));
    } else {
        msg_queue_.pop();
        stop();
    }
}

void zabbixSender::handle_connect(const boost::system::error_code& error,
    tcp::resolver::results_type::iterator endpoint_iter) {
    if (stopped_) return;

    if (!socket_.is_open()) {
        LOG_ERROR << "Connect timed out";
        start_connect(++endpoint_iter);
    } else if (error) {
        LOG_ERROR << "Connect error: " << error.message();
        socket_.close();
        start_connect(++endpoint_iter);
    } else {
        LOG_INFO << "Connected to " << endpoint_iter->endpoint();
        int type = 1;
        std::pair<std::string, std::string> data = msg_queue_.front();
        auto iter = wants_.find(data.first);
        if (iter != wants_.end()) {
            int64_t cur_time = time(NULL);
            if (difftime(cur_time, iter->second) < 600) {
                type = 0;
                send_json_data(data.second);
            } else {
                is_server_want_monitor_data(data.first);
            }
        } else {
            is_server_want_monitor_data(data.first);
        }
        start_read(type);
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
        boost::bind(&zabbixSender::handle_write, shared_from_this(),
            boost::asio::placeholders::error));
}

void zabbixSender::handle_write(const boost::system::error_code& error) {
    if (stopped_) return ;
    
    std::pair<std::string, std::string> data = msg_queue_.front();
    if (!error) {
        LOG_INFO << "send monitor data of host: " << data.first
            << " to server " << server_.host << " success";
    } else {
        LOG_ERROR << "Error on send data: " << error.message()
            << " of host: " << data.first
            << " to server " << server_.host;
        stop();
    }
}

// type == 0 send monitor data | type == 1 query server wants
void zabbixSender::start_read(int type) {
    deadline_.expires_after(std::chrono::seconds(10));

    response_.clear();

    boost::asio::async_read(socket_,
        boost::asio::buffer(response_.data(), zabbixResponse::header_length),
        boost::bind(&zabbixSender::handle_read_header, shared_from_this(),
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred, type));
}

void zabbixSender::handle_read_header(const boost::system::error_code& error, std::size_t n, int type) {
    if (stopped_) return;

    std::pair<std::string, std::string> data = msg_queue_.front();
    if (!error && response_.decode_header()) {
        LOG_INFO << "decode response header of " << data.first
            << " from server " << server_.host << " over";
        
        deadline_.expires_after(std::chrono::seconds(10));

        boost::asio::async_read(socket_,
            boost::asio::buffer(response_.body(), response_.body_length()),
            boost::bind(&zabbixSender::handle_read_body, shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred, type));
    } else {
        LOG_ERROR << "Error on receive header: " << error.message()
            << " of host: " << data.first
            << " from server " << server_.host;

        boost::system::error_code ignored_error;
        socket_.shutdown(tcp::socket::shutdown_both, ignored_error);

        stop();
    }
}

void zabbixSender::handle_read_body(const boost::system::error_code& error, std::size_t n, int type) {
    if (stopped_) return;

    std::pair<std::string, std::string> data = msg_queue_.front();
    if (!error) {
        LOG_INFO << "read zabbix response body " << response_.body();
        bool check = check_response(response_.body());
        if (type == 1) {
            wants_[data.first] = check ? time(NULL) : 0;
            LOG_INFO << "monitor server " << server_.host
                << (check ? " " : " does not ")
                << "need data of host: "
                << data.first;
        } else {
            LOG_INFO << "send monitor data of " << data.first
                << " to server " << server_.host
                << (check ? " success" : " failed");
        }

        if (type == 0 || !check)
            msg_queue_.pop();
    } else {
        LOG_ERROR << "Error on receive body: " << error.message()
            << " of host: " << data.first
            << " from server " << server_.host;
    }

    boost::system::error_code ignored_error;
    socket_.shutdown(tcp::socket::shutdown_both, ignored_error);

    stop();
}

bool zabbixSender::check_response(const char* response) {
    if (!response) return false;
    
    rapidjson::Document doc;
    doc.Parse(response);
    if (!doc.IsObject()) {
        LOG_ERROR << "parse response json failed";
        return false;
    }
    if (!doc.HasMember("response")) {
        LOG_ERROR << "can not find response node";
        return false;
    }
    if (!doc["response"].IsString()) {
        LOG_ERROR << "response node is not a string node";
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
        LOG_ERROR << "deadline expired";

        socket_.close();

        deadline_.expires_at(steady_timer::time_point::max());
    }

    deadline_.async_wait(boost::bind(&zabbixSender::check_deadline, shared_from_this()));
}
