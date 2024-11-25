#include "deeplink_session.h"

#include "deeplink_room.h"
#include "log/log.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/error/error.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/writer.h"

using boost::asio::ip::tcp;

deeplink_session::deeplink_session(boost::asio::ip::tcp::socket socket,
                                   deeplink_room& room)
    : socket_(std::move(socket)), room_(room) {}

deeplink_session::~deeplink_session() {
    LOG_INFO << "deeplink from " << endpoint_ << " destruction";
}

void deeplink_session::start() {
    // room_.join(shared_from_this());
    endpoint_ = socket_.remote_endpoint();
    LOG_INFO << "deeplink from " << endpoint_ << " started";
    room_.join(endpoint_, shared_from_this());
    do_read_header();
}

void deeplink_session::close() {
    socket_.cancel();
    // socket_.shutdown(tcp::socket::shutdown_both);
    socket_.close();
    // socket_.release();
    // socket_.release(boost::asio::error::operation_aborted);
}

void deeplink_session::deliver(const deeplink_lan_message& msg) {
    LOG_INFO << endpoint_ << " deliver " << msg.body();
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress) {
        do_write();
    }
}

void deeplink_session::get_device_info(std::string& device_id,
                                       std::string& password) {
    device_id = device_id_;
    password = password_;
}

void deeplink_session::set_device_info(const std::string& device_id,
                                       const std::string& password) {
    std::stringstream ss;
    ss << "{";
    ss << "\"method\":"
       << "\"setDevicePassword\"";
    ss << ",\"data\":"
       << "{"
       << "\"password\":\"" << password << "\"}";
    ss << "}";
    std::string body = ss.str();
    unsigned int length = body.length();
    deeplink_lan_message message;
    message.body_length(length);
    char* arrLen = reinterpret_cast<char*>(&length);
    std::memcpy(message.data(), arrLen, deeplink_lan_message::header_length);
    std::memcpy(message.body(), body.c_str(), length);
    deliver(message);
}

void deeplink_session::do_read_header() {
    auto self(shared_from_this());
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(read_msg_.data(),
                            deeplink_lan_message::header_length),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec && read_msg_.decode_header()) {
                do_read_body();
            } else {
                // room_.leave(shared_from_this());
                room_.leave(endpoint_);
            }
        });
}

void deeplink_session::do_read_body() {
    auto self(shared_from_this());
    boost::asio::async_read(
        socket_, boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                // room_.deliver(read_msg_);
                LOG_INFO << endpoint_ << ", " << read_msg_.length() << ", "
                         << read_msg_.body();
                rapidjson::Document doc;
                doc.Parse(read_msg_.body());
                if (doc.IsObject()) {
                    std::string method;
                    if (doc.HasMember("method") && doc["method"].IsString()) {
                        method = doc["method"].GetString();
                    }
                    LOG_INFO << "received " << method << " from deeplink";
                    if (method.compare("setDeviceInfo") == 0) {
                        if (doc.HasMember("data") && doc["data"].IsObject()) {
                            const rapidjson::Value& obj = doc["data"];
                            if (obj.HasMember("device_id") &&
                                obj["device_id"].IsString()) {
                                device_id_ = obj["device_id"].GetString();
                            }
                            if (obj.HasMember("password") &&
                                obj["password"].IsString()) {
                                password_ = obj["password"].GetString();
                            }
                        }
                        LOG_INFO << method << " " << device_id_ << " "
                                 << password_;
                    }
                } else {
                    LOG_ERROR << "parse body failed";
                }
                do_read_header();
                // deliver(read_msg_);
            } else {
                // room_.leave(shared_from_this());
                room_.leave(endpoint_);
            }
        });
}

void deeplink_session::do_write() {
    auto self(shared_from_this());
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(write_msgs_.front().data(),
                            write_msgs_.front().length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                write_msgs_.pop_front();
                if (!write_msgs_.empty()) {
                    do_write();
                }
            } else {
                // room_.leave(shared_from_this());
                room_.leave(endpoint_);
            }
        });
}
