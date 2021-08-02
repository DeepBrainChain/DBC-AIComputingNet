#ifndef DBC_TIME_UTIL_H
#define DBC_TIME_UTIL_H

#include <iostream>
#include <string>
#include <ctime>
#include <chrono>

namespace util {
    std::string time_2_str(time_t t);

    std::string time_2_utc(time_t t);

    std::time_t get_time_stamp_ms();
}

#endif
