#ifndef DBC_STRING_UTIL_H
#define DBC_STRING_UTIL_H

#include <iostream>
#include <string>
#include <vector>
#include <boost/asio.hpp>

namespace util {
    void str_split(const std::string & src, const std::string & sep, std::vector<std::string> & vec_str);

    void split(const std::string& str, const std::string& delim, std::vector<std::string>& vec);

    void split(char *str, char delim, int& argc, char *argv[]);

    void trim(std::string& str);

    std::string rtrim(std::string& s, char c);

    std::string ltrim(std::string& s, char c);

    std::string fuzz_ip(const std::string& ip);

    std::string remove_leading_zero(std::string str);
}

#endif //DBC_STRING_UTIL_H
