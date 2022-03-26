#include "string_util.h"

namespace util {
    void split(const std::string &str, const std::string &delim, std::vector<std::string> &vec) {
        size_t pos = 0;
        size_t index = str.find_first_of(delim, pos);
        while (index != std::string::npos) {
            vec.push_back(str.substr(pos, index - pos));

            pos = index + 1;
            index = str.find_first_of(delim, pos);
        }

        std::string left = str.substr(pos, std::string::npos);
        if (left.size() > 0) {
            vec.push_back(left);
        }
    }

    std::vector<std::string> split(const std::string &src, const std::string &sep) {
        std::vector<std::string> vec_str;
        std::string::size_type start = 0;
        for (std::string::size_type end = src.find(sep, start); end != std::string::npos; end = src.find(sep, start)) {
            if (end > start) {
                vec_str.push_back(src.substr(start, end - start));
            }
            start = end + sep.length();
        }
        if (start < src.length()) {
            vec_str.push_back(src.substr(start, src.length() - start));
        }

        return vec_str;
    }

    void trim(std::string &str) {
        if (str.empty()) {
            return;
        }
        
        //left
        if (str[0] == ' ') {
            str.erase(0, str.find_first_not_of(" "));
        }

        //right
        if (str[str.length() - 1] == ' ') {
            str.erase(str.find_last_not_of(" ") + 1);
        }
    }

    std::string rtrim(std::string &s, char c) {
        if (s.size() == 0) {
            return "";
        }

        int i = ((int) s.length()) - 1;

        for (; i >= 0; i--) {
            if (s[i] != c) {
                break;
            }
        }

        return s.substr(0, i + 1);
    }

    std::string ltrim(std::string &s, char c) {
        if (s.size() == 0) {
            return "";
        }

        int i = 0;

        for (; i < s.length(); i++) {
            if (s[i] != c) {
                break;
            }
        }

        return s.substr(i);
    }

    std::string fuzz_ip(const std::string &ip) {
        try {
            boost::asio::ip::address addr = boost::asio::ip::make_address(ip);
            if (addr.is_v4()) {
                boost::asio::ip::address_v4::bytes_type bytes = addr.to_v4().to_bytes();
                return "*.*." + std::to_string(bytes[2]) + "." + std::to_string(bytes[3]);
            } else if (addr.is_v6()) {
                //boost::asio::ip::address_v6::bytes_type bytes = addr.to_v6().to_bytes();
                return "*:*:*:*:*:*:*:*";
            } else {
                return "*.*.*.*";
            }
        }
        catch (const boost::exception &e) {
            return "*.*.*.*";
        }
    }

    std::string remove_leading_zero(std::string str) {
        bool is_input_str_empty = str.empty();
        if (is_input_str_empty) {
            return str;
        }

        str.erase(0, str.find_first_not_of('0'));

        return str.empty() ? "0" : str;
    }

    void replace(std::string& str, const std::string& old_str, const std::string& new_str) {
        std::string::size_type pos = 0;
        while ((pos = str.find(old_str, pos)) != std::string::npos) {
            str.replace(pos, old_str.length(), new_str);
            pos += new_str.length();
        }
    }
}
