#ifndef DBCPROJ_UNITTEST_ZABBIX_SENDER_H
#define DBCPROJ_UNITTEST_ZABBIX_SENDER_H

#include <string>
#include <queue>
#include <boost/asio.hpp>

using boost::asio::steady_timer;
using boost::asio::ip::tcp;
using std::placeholders::_1;
using std::placeholders::_2;

struct monitor_server {
    std::string host;
    std::string port;

    bool operator == (const monitor_server& other) const {
        return host == other.host && port == other.port;
    }

    bool operator<(const monitor_server& other) const {
        if (host < other.host) return true;
        if (host > other.host) return false;
        return port < other.port;
    }
};

class zabbixSender {
public:
    zabbixSender(boost::asio::io_service& io_service,
        const std::string& host, const std::string& port);

    bool start();

    bool stop();

    void push(const std::string& hostname, const std::string& message);

protected:
    void start_connect(tcp::resolver::results_type::iterator endpoint_iter);

    void handle_connect(const boost::system::error_code& error,
        tcp::resolver::results_type::iterator endpoint_iter);
    
    void process_queue();
    
    void send_json_data(const std::string &data);

    void is_server_want_monitor_data(const std::string& hostname);

    void start_write(const std::string &data);

    void handle_write(const boost::system::error_code& error);

    // type == 0 send monitor data | type == 1 query server wants
    void start_read(int type);

    void handle_read(const boost::system::error_code& error, std::size_t n, int type);

    void do_read(const std::string& response, int type);

    const char* get_json_string(const char* result, unsigned long long &jsonLength);

    bool check_response(const char* response);

    void check_deadline();

private:
    const int max_length_ = 4096;
    bool stopped_ = false;
    tcp::resolver resolver_;
    tcp::resolver::results_type endpoints_;
    tcp::socket socket_;
    std::string input_buffer_;
    steady_timer deadline_;
    steady_timer queue_timer_;
    std::queue<std::pair<std::string, std::string>> msg_queue_;
    std::map<std::string, bool> wants_;
    monitor_server server_;
};

#endif
