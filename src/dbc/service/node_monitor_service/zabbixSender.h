#ifndef DBC_ZABBIX_SENDER_H
#define DBC_ZABBIX_SENDER_H

#include <string>
#include <queue>
#include <boost/asio.hpp>

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
    
    void send_json_data(const std::string &data);

    void is_server_want_monitor_data(const std::string& hostname);

    void start_write(const std::string &data);

    void handle_write(const boost::system::error_code& error);

    // type == 0 send monitor data | type == 1 query server wants
    void start_read(int type);

    void handle_read(const boost::system::error_code& error, std::size_t n, int type);

    const char* get_json_string(const char* result, unsigned long long &jsonLength);

    bool check_response(const char* response);

    void check_deadline();

private:
    const int max_length_ = 4096;
    bool stopped_ = false;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ip::tcp::resolver::results_type endpoints_;
    boost::asio::ip::tcp::socket socket_;
    std::string input_buffer_;
    boost::asio::steady_timer deadline_;
    zabbix_server server_;
    boost::asio::steady_timer queue_timer_;
    std::queue<std::pair<std::string, std::string>> msg_queue_;
    std::queue<std::pair<std::string, std::string>> wait_queue_;
    std::map<std::string, int64_t> wants_;

};

#endif
