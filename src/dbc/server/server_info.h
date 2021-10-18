#ifndef DBC_SERVER_INFO_H
#define DBC_SERVER_INFO_H

#include <vector>
#include <string>

class server_info {
public:
    server_info();

    void add_service_list(std::string s) {m_service_list.push_back(s);}

    std::vector<std::string>& get_service_list() { return m_service_list;}

    server_info& operator = (const server_info& b);

private:
    std::vector<std::string> m_service_list;
};

#endif
