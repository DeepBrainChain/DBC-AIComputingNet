#ifndef DBC_HOST_MONITOR_INFO_H
#define DBC_HOST_MONITOR_INFO_H

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string>
#include <vector>
#include <map>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

namespace dbcMonitor {
struct gpuInfo;

struct hostGpuInfo {
    std::string name;               // the product name of this device.
    std::string busId;              // The tuple domain:bus:device.function PCI identifier (NULL terminator)
    unsigned long long memTotal;    // Total physical device memory (in bytes)
    unsigned long long memFree;     // Unallocated device memory (in bytes)
    unsigned long long memUsed;     // Sum of Reserved and Allocated device memory (in bytes).
    unsigned int gpuUtilization;    // Percent of time over the past sample period during which one or more kernels was executing on the GPU
    unsigned int memUtilization;    // Percent of time over the past sample period during which global (device) memory was being read or written
    unsigned int powerUsage;        // the power usage information in milliwatts
    unsigned int powerCap;          // the power management limit in milliwatts
    unsigned int temperature;       // the current temperature readings for the device, in degrees C.
    struct timespec realTime;

    hostGpuInfo& operator=(const gpuInfo& gpu);
    // void write2Json(rapidjson::Writer<rapidjson::StringBuffer> &write) const;
    void write2ZabbixJson(rapidjson::Writer<rapidjson::StringBuffer> &write, const std::string &host, std::string index) const;
};

struct hostMonitorData{
    std::string nodeId;                             // 机器ID
    unsigned int delay;                             // 间隔时间，例如每10秒收集一次
    unsigned int gpuCount;                          // GPU数量
    unsigned int gpuUsed;                           // GPU已使用
    std::map<std::string, hostGpuInfo> gpuStats;    // GPU数据
    unsigned int vmCount;                           // VM数量
    unsigned int vmRunning;                         // 运行中的VM数量
    float cpuUsage;                                 // CPU使用率，已经乘以100
    // memoryStats memStats;                           // 内存数据
    unsigned long long memTotal;                    // 内存total，单位KB
    unsigned long long memFree;                     // 内存unused，单位KB
    // unsigned long long memAvailable;                // 内存available，单位KB
    float memUsage;                                 // 内存使用率，已经乘以100
    long long rxFlow;                               // 流入流量
    long long txFlow;                               // 流出流量
    unsigned long long diskTotal;                   // 数据盘大小
    unsigned long long diskFree;                    // 数据盘free大小
    float diskUsage;                                // 数据盘使用率，已经乘以100
    std::string diskMountStatus;                    // 数据盘挂载状态, "normal" or "lost"
    std::vector<float> loadAverage;                 // 负载平均值
    std::vector<float> packetLossRate;              // 丢包率(CU|CT|CM)
    std::string version;                            // 版本号

    std::string toJsonString() const;
    std::string toZabbixString() const;
};
}

#endif
