#ifndef DBC_TASK_MONITOR_INFO_H
#define DBC_TASK_MONITOR_INFO_H

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string>
#include <map>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

// 若想快速查询数据请使用命令 virsh domstats <domain> 或者 virDomainListGetStats

namespace dbcMonitor {
struct domainInfo {
    std::string state;             // 虚拟机的状态
    unsigned long maxMem;          // 虚拟机的最大内存，单位KB
    unsigned long memory;          // 虚拟机已使用的内存大小，单位KB。可能与maxMem相等，不建议使用
    unsigned short nrVirtCpu;      // 虚拟机的CPU个数
    unsigned long long cpuTime;    // CPU时间，单位纳秒
    struct timespec realTime;      // 实际时间
    float cpuUsage;                // CPU使用率，已经乘以100，不确定是否计算
    // unsigned int delay;

    void calculatorUsage(const domainInfo &last);
    void write2Json(rapidjson::Writer<rapidjson::StringBuffer> &write) const;
    void write2ZabbixJson(rapidjson::Writer<rapidjson::StringBuffer> &write, const std::string &host) const;
};

struct cpuStats {
    unsigned long long cpu_time;
    unsigned long long user_time;
    unsigned long long system_time;
    unsigned long long vcpu_time;
    // struct timespec realTime;
    // unsigned int delay;

    // std::string toJsonString();
};

struct memoryStats {
    unsigned long long total;        // 内存total，单位KB
    unsigned long long unused;       // 内存unused，单位KB
    unsigned long long available;    // 内存available，单位KB
    float usage;                     // 内存使用率，已经乘以100
    struct timespec realTime;        // 实际时间
    // unsigned int delay;

    void calculatorUsage(const memoryStats &last);
    void write2Json(rapidjson::Writer<rapidjson::StringBuffer> &write) const;
    void write2ZabbixJson(rapidjson::Writer<rapidjson::StringBuffer> &write, const std::string &host) const;
};

struct diskInfo {
    std::string name;                 // 磁盘名称，例如'vda'
    unsigned long long capacity;      // 磁盘逻辑大小，单位字节
    unsigned long long allocation;    // 磁盘存储大小，单位字节，类似'du'命令
    unsigned long long physical;      // 磁盘物理大小，单位字节，类似'ls'命令
    long long rd_req;                 // number of read requests
    long long rd_bytes;               // number of read bytes
    long long wr_req;                 // number of write requests
    long long wr_bytes;               // number of written bytes
    long long errs;                   // In Xen this returns the mysterious 'oo_req'.
    float rd_speed;                   // 磁盘读取速度，单位B/s? 不确定是否计算
    float wr_speed;                   // 磁盘写入速度，单位B/s? 不确定是否计算
    struct timespec realTime;         // 实际时间
    // unsigned int delay;

    void calculatorSpeed(const diskInfo &last);
    void write2Json(rapidjson::Writer<rapidjson::StringBuffer> &write) const;
    void write2ZabbixJson(rapidjson::Writer<rapidjson::StringBuffer> &write, const std::string &host, std::string index) const;
};

struct networkInfo {
    std::string name;            // 网卡名称，例如'vnet0'
    long long rx_bytes;          // 接收的字节数
    long long rx_packets;        // 接收的包
    long long rx_errs;
    long long rx_drop;
    long long tx_bytes;          // 发送的字节数
    long long tx_packets;        // 发送的包
    long long tx_errs;
    long long tx_drop;
    float rx_speed;              // 接收速度，单位B/s? 不确定是否计算
    float tx_speed;              // 发送速度，单位B/s? 不确定是否计算
    struct timespec realTime;    // 实际时间
    // unsigned int delay;

    void calculatorSpeed(const networkInfo &last);
    void write2Json(rapidjson::Writer<rapidjson::StringBuffer> &write) const;
    void write2ZabbixJson(rapidjson::Writer<rapidjson::StringBuffer> &write, const std::string &host, std::string index) const;
};

struct gpuInfo {
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

    void calculatorUsage(const gpuInfo &last);
    void write2Json(rapidjson::Writer<rapidjson::StringBuffer> &write) const;
    void write2ZabbixJson(rapidjson::Writer<rapidjson::StringBuffer> &write, const std::string &host, std::string index) const;
};

struct domMonitorData{
    std::string domainName;                         // 虚拟机的名称
    unsigned int delay;                             // 间隔时间，例如每10秒收集一次
    domainInfo domInfo;                             // 虚拟机的基本信息
    // cpuStats cpuStats;                           // CPU数据
    memoryStats memStats;                           // 内存数据
    std::map<std::string, diskInfo> diskStats;      // 磁盘数据
    std::map<std::string, networkInfo> netStats;    // 网络数据
    std::string graphicsDriverVersion;              // the version of the system's graphics driver
    std::string nvmlVersion;                        // the version of the NVML library
    std::string cudaVersion;                        // the version of the CUDA driver
    std::map<std::string, gpuInfo> gpuStats;        // GPU数据
    std::string version;                            // 版本号

    void calculatorUsageAndSpeed(const domMonitorData &last);
    std::string toJsonString() const;
    std::string toZabbixString() const;
};
}

#endif
