#ifndef DBCPROJ_UNITTEST_ZABBIX_SENDER_H
#define DBCPROJ_UNITTEST_ZABBIX_SENDER_H

#include <string>

class zabbixSender {
public:
    zabbixSender(const std::string &server, const std::string &port);

    bool sendJsonData(const std::string &data);

    bool is_server_want_monitor_data(const std::string& hostname);

protected:
    void sendData(const std::string &data, std::string &reply);

    const char* getJsonString(const char* result, unsigned long long &jsonLength);

    bool checkResponse(const std::string &reply);

private:
    std::string server_;
    std::string port_;
};

#endif
