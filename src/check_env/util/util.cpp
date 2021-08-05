#include "util.h"
#include "string_util.h"

std::string size_to_string(int64_t n, int64_t scale) {
    int64_t rest = 0;
    n = n * scale;
    int64_t size = n;

    if(size < 1024) {
        return std::to_string(size) + "B";
    } else {
        size /= 1024;
        rest = n - size * 1024;
    }

    if(size < 1024) {
        return std::to_string(size) + "." + std::to_string(rest * 100).substr(0, 2) + "KB";
    } else {
        size /= 1024;
        rest = n - size * 1024 * 1024;
    }

    if(size < 1024) {
        return std::to_string(size) + "." + std::to_string(rest * 100).substr(0, 2) + "MB";
    } else {
        size /= 1024;
        rest = n - size * 1024 * 1024 * 1024;
    }

    if (size < 1024) {
        return std::to_string(size) + "." + std::to_string(rest * 100).substr(0, 2) + "GB";
    } else {
        size /= 1024;
        rest = n - size * 1024 * 1024 * 1024 * 1024;
        return std::to_string(size) + "." + std::to_string(rest * 100).substr(0, 2) + "TB";
    }
}

std::string get_public_ip() {
    int try_count = 0;
    std::string public_ip = run_shell("curl -s myip.ipip.net | awk -F ' ' '{print $2}' | awk -F '：' '{print $2}'");
    public_ip = util::rtrim(public_ip, '\n');
    while (public_ip.empty() && try_count < 10) {
        public_ip = run_shell("curl -s myip.ipip.net | awk -F ' ' '{print $2}' | awk -F '：' '{print $2}'");
        public_ip = util::rtrim(public_ip, '\n');
        if (!public_ip.empty()) {
            break;
        }
        try_count++;
        sleep(1);
    }

    return public_ip;
}

int32_t str_to_i32(const std::string& str) {
    try {
        int32_t n = std::stoi(str);
        return n;
    } catch (std::exception& e) {
        return 0;
    }
}

uint32_t str_to_u32(const std::string& str) {
    try {
        uint64_t n = std::stoul(str);
        return (uint32_t)n;
    } catch (std::exception& e) {
        return 0;
    }
}

int64_t str_to_i64(const std::string& str) {
    try {
        int64_t n = std::stol(str);
        return n;
    } catch (std::exception& e) {
        return 0;
    }
}

uint64_t str_to_u64(const std::string& str) {
    try {
        uint64_t n = std::stoul(str);
        return n;
    } catch (std::exception& e) {
        return 0;
    }
}

float str_to_f(const std::string& str) {
    try {
        float n = std::stof(str);
        return n;
    } catch (std::exception& e) {
        return 0;
    }
}
