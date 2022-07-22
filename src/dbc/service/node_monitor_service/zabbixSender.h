#ifndef DBC_ZABBIX_SENDER_H
#define DBC_ZABBIX_SENDER_H

#include <string>
#include <queue>
#include <boost/asio.hpp>

class zabbixResponse {
public: 
    enum { header_length = 13 };
    enum { max_body_length = 4096 };

    zabbixResponse();

    const char* data() const;

    char* data();

    std::size_t length() const;

    const char* body() const;

    char* body();

    std::size_t body_length() const;

    bool decode_header();

    void clear();

private:
    std::string data_;
    std::size_t body_length_;
};

///////////////////////////////////////////////////////////////////////////////

class zabbixSender : public std::enable_shared_from_this<zabbixSender> {
public:
    zabbixSender(boost::asio::io_service& io_service,
        const std::string& host, const std::string& port);

    void start();

    void stop_graceful();

    void stop();

    void push(const std::string& hostname, const std::string& json);

public:
    struct zabbix_server {
        std::string host;
        std::string port;
    };

protected:
    void queue_process();

    void start_connect(boost::asio::ip::tcp::resolver::results_type::iterator endpoint_iter);

    void handle_connect(const boost::system::error_code& error,
        boost::asio::ip::tcp::resolver::results_type::iterator endpoint_iter);

    void send_json_data(const std::string& hostname, const std::string &data);

    void is_server_want_monitor_data(const std::string& hostname);

    void start_write(const std::string& hostname, const std::string &data);

    void handle_write(const boost::system::error_code& error,
        const std::string& hostname);

    // type == 0 send monitor data | type == 1 query server wants
    void start_read(const std::string& hostname, int type);

    void handle_read_header(const boost::system::error_code& error, std::size_t n,
        const std::string& hostname, int type);

    void handle_read_body(const boost::system::error_code& error, std::size_t n,
        const std::string& hostname, int type);

    const char* get_json_string(const char* result, unsigned long long &jsonLength);

    bool check_response(const char* response);

    void check_deadline();

private:
    bool stopped_ = false;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ip::tcp::resolver::results_type endpoints_;
    boost::asio::ip::tcp::socket socket_;
    zabbixResponse response_;
    boost::asio::steady_timer deadline_;
    boost::asio::steady_timer queue_timer_;
    std::queue<std::pair<std::string, std::string>> msg_queue_;
    std::map<std::string, int64_t> wants_;
    zabbix_server server_;
};

#endif
