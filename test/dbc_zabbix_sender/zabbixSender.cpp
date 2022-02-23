#include "zabbixSender.h"
#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <string.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#define ZBX_TCP_HEADER_DATA "ZBXD"

zabbixSender::zabbixSender(const std::string &server, const std::string &port)
    : server_(server)
    , port_(port) {

}

bool zabbixSender::sendJsonData(const std::string &json_data) {
    std::string data = "ZBXD\x01";
    unsigned long long data_len = json_data.length();
    char* arrLen = reinterpret_cast<char*>(&data_len);
    data.append(arrLen, 8);
    data.append(json_data);
    std::string reply;
    sendData(data, reply);
    std::cout << "zabbix reply: " << reply << std::endl;
    return checkResponse(reply);
}

bool zabbixSender::is_server_want_monitor_data(const std::string& hostname) {
    rapidjson::StringBuffer strBuf;
    rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
    write.StartObject();
    write.Key("request");
    write.String("active checks");
    write.Key("host");
    write.String(hostname.c_str());
    write.EndObject();
    std::string json_data = strBuf.GetString();
    return sendJsonData(json_data);
}

void zabbixSender::sendData(const std::string &data, std::string &reply) {
    try {
        boost::asio::io_service io_service;
        boost::asio::ip::tcp::resolver resolver(io_service);
        boost::asio::ip::tcp::resolver::query query(server_, port_);
        boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);
        boost::asio::ip::tcp::resolver::iterator end;

        //建立一个socket并且连接它
        boost::asio::ip::tcp::socket socket(io_service);
        boost::asio::connect(socket, iterator);

        //输出查看所有查询到的服务器
        // while (iterator != end) {
        //     boost::asio::ip::tcp::endpoint endpoint = *iterator++;
        //     std::cout << endpoint << std::endl;
        // }

        // boost::array<char, 128> buf;
        boost::system::error_code error;
        std::size_t ret;
        // socket.write_some(boost::asio::buffer(data), error);
        ret = boost::asio::write(socket, boost::asio::buffer(data), error);
        if (error) {
            std::cout << "send data error: " << error.message() << ", data size:" << data.length() << std::endl;
        } else {
            char buf[4096] = {0};
            // socket.read_some(boost::asio::buffer(buf), error);
            ret = boost::asio::read(socket, boost::asio::buffer(buf), error);
            if (error) {
                std::cerr << "read reply error: " << error.message() << std::endl;
            }
            // std::cout << "zabbix reply raw data length: " << ret << ", data: " << buf << std::endl;
            unsigned long long jsonLength = 0;
            const char* jsonstr = getJsonString(buf, jsonLength);
            if (jsonstr && jsonLength > 0 && ret > jsonLength) {
                reply.resize(jsonLength);
                memcpy(&reply[0], jsonstr, jsonLength);
            } else {
                // std::cout << "parse zabbix reply error" << std::endl;
                std::cout << "parse zabbix reply error(string begin: " << jsonstr << ", string length: " << jsonLength << ")" << std::endl;
            }
        }
    } catch (std::exception &e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }
}

bool zabbixSender::checkResponse(const std::string &reply) {
    rapidjson::Document doc;
    doc.Parse(reply.c_str());
    if (!doc.IsObject()) return false;
    if (!doc.HasMember("response")) return false;
    if (!doc["response"].IsString()) return false;
    std::string response = doc["response"].GetString();
    return response == "success";
}

const char* zabbixSender::getJsonString(const char* result, unsigned long long &jsonLength) {
    const char* zbx_tcp_header = strstr(result, ZBX_TCP_HEADER_DATA);
    if (!zbx_tcp_header) return nullptr;
    jsonLength = *reinterpret_cast<const unsigned long long*>(zbx_tcp_header + 5);
    return zbx_tcp_header + 5 + 8;
}
