#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <deque>
#include <iostream>

#include "chat_message.hpp"
#include "common/common.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/error/error.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/writer.h"

using boost::asio::ip::tcp;

// default server
static const char* default_host = "0.0.0.0";
static short default_port = 10051;

boost::asio::io_context io_context;

void signal_handler(int sig) {
    std::cout << "catch signal and wait for quit..." << std::endl;
    io_context.stop();
}

class chat_participant {
public:
    virtual ~chat_participant() {}
    virtual void close() = 0;
    virtual void deliver(const chat_message& msg) = 0;
};

class chat_room {
public:
    // void join(std::shared_ptr<chat_session> participant) {
    //     participants_.insert(participant);
    // }
    // void leave(std::shared_ptr<chat_session> participant) {
    //     participants_.erase(participant);
    // }
    void join(const tcp::endpoint& endpoint,
              std::shared_ptr<chat_participant> participant) {
        std::cout << endpoint << " join" << std::endl;
        // auto iter = participants_.find(endpoint);
        typedef std::pair<tcp::endpoint, std::shared_ptr<chat_participant>>
            MapType;
        auto iter =
            std::find_if(participants_.begin(), participants_.end(),
                         [&endpoint](const MapType& it) {
                             return it.first.address() == endpoint.address();
                         });
        if (iter != participants_.end()) {
            iter->second->close();
        }
        participants_[endpoint] = participant;
    }
    void leave(const tcp::endpoint& endpoint) {
        std::cout << endpoint << " leave" << std::endl;
        participants_.erase(endpoint);
    }

private:
    // std::set<std::shared_ptr<chat_session>> participants_;
    std::map<tcp::endpoint, std::shared_ptr<chat_participant>> participants_;
};

class chat_session : public chat_participant,
                     public std::enable_shared_from_this<chat_session> {
public:
    chat_session(tcp::socket socket, chat_room& room)
        : socket_(std::move(socket)), room_(room) {
        std::cout << "chat session construction" << std::endl;
    }
    ~chat_session() { std::cout << "chat session destruction" << std::endl; }

    void start() {
        // room_.join(shared_from_this());
        endpoint_ = socket_.remote_endpoint();
        room_.join(endpoint_, shared_from_this());
        do_read_header();
    }

    void close() {
        socket_.cancel();
        // socket_.shutdown(tcp::socket::shutdown_both);
        socket_.close();
        // socket_.release();
        // socket_.release(boost::asio::error::operation_aborted);
    }

    void deliver(const chat_message& msg) {
        bool write_in_progress = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!write_in_progress) {
            do_write();
        }
    }

private:
    void do_read_header() {
        auto self(shared_from_this());
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(read_msg_.data(), chat_message::header_length),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec && read_msg_.decode_header()) {
                    do_read_body();
                } else {
                    // room_.leave(shared_from_this());
                    room_.leave(endpoint_);
                }
            });
    }

    void do_read_body() {
        auto self(shared_from_this());
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    // room_.deliver(read_msg_);
                    std::cout << socket_.remote_endpoint() << ", "
                              << read_msg_.length() << ", " << read_msg_.body()
                              << std::endl;
                    rapidjson::Document doc;
                    doc.Parse(read_msg_.body());
                    if (doc.IsObject()) {
                        std::string method, device_id, password;
                        if (doc.HasMember("method") &&
                            doc["method"].IsString()) {
                            method = doc["method"].GetString();
                        }
                        if (doc.HasMember("data") && doc["data"].IsObject()) {
                            const rapidjson::Value& obj = doc["data"];
                            if (obj.HasMember("device_id") &&
                                obj["device_id"].IsString()) {
                                device_id = obj["device_id"].GetString();
                            }
                            if (obj.HasMember("password") &&
                                obj["password"].IsString()) {
                                password = obj["password"].GetString();
                            }
                        }
                        std::cout << method << " " << device_id << " "
                                  << password << std::endl;
                    } else {
                        std::cout << "parse body failed" << std::endl;
                    }
                    do_read_header();
                    deliver(read_msg_);
                } else {
                    // room_.leave(shared_from_this());
                    room_.leave(endpoint_);
                }
            });
    }

    void do_write() {
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

    tcp::endpoint endpoint_;
    tcp::socket socket_;
    chat_room& room_;
    chat_message read_msg_;
    std::deque<chat_message> write_msgs_;
};

class chat_server {
public:
    chat_server(boost::asio::io_context& io_context, const std::string& host,
                short port)
        : acceptor_(io_context,
                    tcp::endpoint(boost::asio::ip::address::from_string(host),
                                  port)) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<chat_session>(std::move(socket), room_)
                        ->start();
                }

                do_accept();
            });
    }
    tcp::acceptor acceptor_;
    chat_room room_;
};

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);

    std::string host = default_host;
    short port = default_port;

    boost::program_options::options_description opts(
        "collectMonitorData command options");
    opts.add_options()("host", boost::program_options::value<std::string>(),
                       "server, such as 0.0.0.0")(
        "port", boost::program_options::value<short>(), "port, such as 10051")(
        "help", "command options help");

    try {
        boost::program_options::variables_map vm;
        boost::program_options::store(
            boost::program_options::parse_command_line(argc, argv, opts), vm);
        boost::program_options::notify(vm);

        if (vm.count("host")) {
            host = vm["host"].as<std::string>();
        }
        if (vm.count("port")) {
            port = vm["port"].as<short>();
        }
        std::cout << "host:" << host << ", port:" << port << std::endl;
        if (vm.count("help") > 0) {
            std::cout << opts;
            return 0;
        }
    } catch (const std::exception& e) {
        std::cout << "invalid command option " << e.what() << std::endl;
        std::cout << opts;
        return 0;
    }

    // boost::asio::io_context io_context;
    chat_server server(io_context, host, port);

    io_context.run();

    return 0;
}
