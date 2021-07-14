#ifndef DBC_COMM_H
#define DBC_COMM_H

#include <list>
#include <set>
#include "util.h"
#include <boost/program_options.hpp>
#include "time_util.h"
#include "log.h"

class cmd_peer_node_log {
public:

    std::string peer_node_id;

    std::string log_content;

};

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

#endif //DBC_COMM_H
