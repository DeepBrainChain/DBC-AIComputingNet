#ifndef DBC_STRING_UTIL_H
#define DBC_STRING_UTIL_H

#include <iostream>
#include <string>
#include <vector>
#include <boost/asio.hpp>

namespace util {
    void split(const std::string& str, const std::string& delim, std::vector<std::string>& vec);

    std::vector<std::string> split(const std::string &src, const std::string &sep);

    void trim(std::string& str);

    std::string rtrim(std::string& s, char c);

    std::string ltrim(std::string& s, char c);

    std::string fuzz_ip(const std::string& ip);

    std::string remove_leading_zero(std::string str);

    void replace(std::string& str, const std::string& old_str, const std::string& new_str);

    inline bool is_digits(const std::string& s) {
        if(s.length() == 0) return false;
        for (size_t i = 0; i < s.length(); i++){
            if (!isdigit(s.c_str()[i])) return false;
        }
        return true;
    }
}

#endif //DBC_STRING_UTIL_H
