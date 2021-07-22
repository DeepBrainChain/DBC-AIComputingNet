#include "time_util.h"
#include <cstring>

namespace util {
    std::string time_2_str(time_t t)
    {
        struct tm _tm;
        localtime_r(&t, &_tm);
        char buf[256];
        memset(buf, 0, sizeof(char) * 256);
        strftime(buf, sizeof(char) * 256, "%x %X", &_tm);

        return buf;
    }

    std::string time_2_utc(time_t t)
    {
        std::string temp = std::asctime(std::gmtime(&t));

        return temp.erase(temp.find("\n"));
    }

    std::time_t get_time_stamp_ms()
    {
        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        auto tmp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
        std::time_t time_stamp = tmp.count();
        return time_stamp;
    }
}