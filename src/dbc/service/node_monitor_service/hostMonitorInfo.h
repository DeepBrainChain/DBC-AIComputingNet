#ifndef DBC_HOST_MONITOR_INFO_H
#define DBC_HOST_MONITOR_INFO_H

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string>
#include <vector>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

namespace dbcMonitor {
struct hostMonitorData{
    std::string nodeId;                             // 机器ID
    unsigned int delay;                             // 间隔时间，例如每10秒收集一次
    unsigned int gpuCount;                          // GPU数量
    unsigned int gpuUsed;                           // GPU已使用
    unsigned int vmCount;                           // VM数量
    unsigned int vmRunning;                         // 运行中的VM数量
    float cpuUsage;                                 // CPU使用率
    // memoryStats memStats;                           // 内存数据
    unsigned long long memTotal;                    // 内存total，单位KB
    unsigned long long memFree;                     // 内存unused，单位KB
    // unsigned long long memAvailable;                // 内存available，单位KB
    float memUsage;                                 // 内存使用率，已经乘以100
    unsigned long long rxFlow;                      // 流入流量
    unsigned long long txFlow;                      // 流出流量
    unsigned long long diskTotal;                   // 数据盘大小
    unsigned long long diskFree;                    // 数据盘free大小
    float diskUsage;                                // 数据盘使用率
    std::vector<float> loadAverage;                 // 负载平均值
    std::vector<float> packetLossRate;              // 丢包率(CU|CT|CM)
    std::string version;                            // 版本号

    std::string toJsonString() const;
    std::string toZabbixString() const;
};
}

#endif
