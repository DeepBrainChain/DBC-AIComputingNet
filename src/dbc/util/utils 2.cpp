#include "utils.h"

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

