#ifndef DBCPROJ_UTIL_H
#define DBCPROJ_UTIL_H

#include <iostream>
#include <string>
#include <cerrno>
#include <cstring>
#include "string_util.h"
#include "TToString.h"
#include "singleton.h"
#include "resource_def.h"

static const char* color_blue = "\033[1;34m";
static const char* color_red = "\033[1;31m";
static const char* color_green = "\033[1;32m";
static const char* color_reset = "\033[0m";

    #define print(...) \
    do {                       \
    char line[1024] = {0}; \
    snprintf(line, 1024, __VA_ARGS__); \
    printf("%s\n", line); \
    } while(0);

    #define print_blue(...) \
    do {                            \
    char line[1024] = {0}; \
    snprintf(line, 1024, __VA_ARGS__); \
    printf("%s%s%s\n", color_blue, line, color_reset); \
    } while(0);

    #define print_red(...)  \
    do {                            \
    char line[1024] = {0}; \
    snprintf(line, 1024, __VA_ARGS__); \
    printf("%s%s%s\n", color_red, line, color_reset); \
    } while(0);

    #define print_green(...)\
    do {                            \
    char line[1024] = {0}; \
    snprintf(line, 1024, __VA_ARGS__); \
    printf("%s%s%s\n", color_green, line, color_reset); \
    } while(0);

static std::string run_shell(const char* cmd, const char* modes = "r") {
    if (cmd == nullptr) return "";

    FILE * fp = nullptr;
    char buffer[1024] = {0};
    fp = popen(cmd, modes);
    if (fp != nullptr) {
        fgets(buffer, sizeof(buffer), fp);
        pclose(fp);
        return std::string(buffer);
    } else {
        return std::string("run_shell failed! ") + cmd + std::string(" ") + std::to_string(errno) + ":" + strerror(errno);
    }
}

static std::string read_info(const char* file, const char* modes = "r") {
    FILE *fp = fopen(file, modes);
    if(NULL == fp) return "";

    char szBuf[1024] = {0};
    fgets(szBuf, sizeof(szBuf) - 1, fp);
    fclose(fp);

    return std::string(szBuf);
}

// B => TB GB KB
std::string size_to_string(int64_t n, int64_t scale = 1);

// 获取公网ip地址
std::string get_public_ip();

// string to digit
int32_t str_to_i32(const std::string& str);
uint32_t str_to_u32(const std::string& str);
int64_t str_to_i64(const std::string& str);
uint64_t str_to_u64(const std::string& str);
float str_to_f(const std::string& str);

#endif //DBCPROJ_UTIL_H
