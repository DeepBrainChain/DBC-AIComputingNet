#include "TaskMonitorInfo.h"
#include <cmath>
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"

namespace dbcMonitor {
// 保留两位小数
inline float roundFloat(float value) {
    return round(value * 100) / 100.0f;
}

class rappidJsonWriteFormat {
public:
    rappidJsonWriteFormat(rapidjson::Writer<rapidjson::StringBuffer> *w) : write(w) {}

    void formatValue(int value) {write->Int(value);}
    void formatValue(long long value) {write->Int64(value);}
    void formatValue(unsigned int value) {write->Uint(value);}
    void formatValue(unsigned long long value) {write->Uint64(value);}
    void formatValue(float value) {write->Double(value);}
    void formatValue(double value) {write->Double(value);}
    void formatValue(bool value) {write->Bool(value);}
    void formatValue(const char *value) {write->String(value);}
    void formatValue(const std::string &value) {write->String(value.c_str());}
private:
    rapidjson::Writer<rapidjson::StringBuffer> *write;
};

template<class T>
void writeZabbixJsonItem(rapidjson::Writer<rapidjson::StringBuffer> &write, const std::string &host,
                         const std::string &key, const T &value, const timespec &time) {
    write.StartObject();
    write.Key("host");
    write.String(host.c_str());
    write.Key("key");
    write.String(key.c_str());
    write.Key("value");
    rappidJsonWriteFormat rjwf(&write);
    rjwf.formatValue(value);
    write.Key("clock");
    write.Int64(time.tv_sec);
    write.Key("ns");
    write.Int(time.tv_nsec);
    write.EndObject();
}

void domainInfo::calculatorUsage(const domainInfo &last) {
    // 转换成微秒来计算
    int cpu_diff = (cpuTime - last.cpuTime) / 1000;
    if (cpu_diff > 0) {
        int real_diff = 1000000 * (realTime.tv_sec - last.realTime.tv_sec) + (realTime.tv_nsec - last.realTime.tv_nsec) / 1000;
        cpuUsage = 100 * cpu_diff / (float) (real_diff) / nrVirtCpu;
    } else {
        cpuUsage = 0.0f;
    }
}

void domainInfo::write2Json(rapidjson::Writer<rapidjson::StringBuffer> &write) const {
    write.Key("domInfo");
    write.StartObject();
    write.Key("state");
    write.String(state.c_str());
    write.Key("maxMem");
    write.Uint(maxMem);
    write.Key("memory");
    write.Uint(memory);
    write.Key("nrVirtCpu");
    write.Uint(nrVirtCpu);
    write.Key("cpuTime");
    write.Uint64(cpuTime);
    write.Key("clock");
    write.Int64(realTime.tv_sec);
    write.Key("ns");
    write.Int(realTime.tv_nsec);
    write.Key("cpuUsage");
    write.Double(cpuUsage);
    write.EndObject();
}

void domainInfo::write2ZabbixJson(rapidjson::Writer<rapidjson::StringBuffer> &write, const std::string &host) const {
    writeZabbixJsonItem<std::string>(write, host, "dom.state", state, realTime);
    writeZabbixJsonItem<unsigned int>(write, host, "dom.maxMem", maxMem, realTime);
    writeZabbixJsonItem<unsigned int>(write, host, "dom.memory", memory, realTime);
    writeZabbixJsonItem<unsigned int>(write, host, "dom.nrVirtCpu", nrVirtCpu, realTime);
    writeZabbixJsonItem<unsigned long long>(write, host, "dom.cpuTime", cpuTime, realTime);
    writeZabbixJsonItem<float>(write, host, "dom.cpuUsage", cpuUsage, realTime);
}

void memoryStats::calculatorUsage(const memoryStats &last) {
    usage = 100 * (float)(total - unused) / total;
}

void memoryStats::write2Json(rapidjson::Writer<rapidjson::StringBuffer> &write) const {
    write.Key("memory");
    write.StartObject();
    write.Key("total");
    write.Uint64(total);
    write.Key("unused");
    write.Uint64(unused);
    write.Key("available");
    write.Uint64(available);
    write.Key("usage");
    write.Double(usage);
    write.Key("clock");
    write.Int64(realTime.tv_sec);
    write.Key("ns");
    write.Int(realTime.tv_nsec);
    write.EndObject();
}

void memoryStats::write2ZabbixJson(rapidjson::Writer<rapidjson::StringBuffer> &write, const std::string &host) const {
    writeZabbixJsonItem<unsigned long long>(write, host, "memory.total", total, realTime);
    writeZabbixJsonItem<unsigned long long>(write, host, "memory.unused", unused, realTime);
    writeZabbixJsonItem<unsigned long long>(write, host, "memory.available", available, realTime);
    writeZabbixJsonItem<float>(write, host, "memory.usage", usage, realTime);
}

void diskInfo::calculatorSpeed(const diskInfo &last) {
    int time_delta = realTime.tv_sec - last.realTime.tv_sec;
    rd_speed = (rd_bytes - last.rd_bytes) / (float)time_delta;
    wr_speed = (wr_bytes - last.wr_bytes) / (float)time_delta;
}

void diskInfo::write2Json(rapidjson::Writer<rapidjson::StringBuffer> &write) const {
    write.StartObject();
    write.Key("disk_name");
    write.String(name.c_str());
    write.Key("capacity");
    write.Uint64(capacity);
    write.Key("allocation");
    write.Uint64(allocation);
    write.Key("physical");
    write.Uint64(physical);
    write.Key("rd_req");
    write.Int64(rd_req);
    write.Key("rd_bytes");
    write.Int64(rd_bytes);
    write.Key("wr_req");
    write.Int64(wr_req);
    write.Key("wr_bytes");
    write.Int64(wr_bytes);
    write.Key("errs");
    write.Int64(errs);
    write.Key("rd_speed");
    write.Double(rd_speed);
    write.Key("wr_speed");
    write.Double(wr_speed);
    write.Key("clock");
    write.Int64(realTime.tv_sec);
    write.Key("ns");
    write.Int(realTime.tv_nsec);
    write.EndObject();
}

void diskInfo::write2ZabbixJson(rapidjson::Writer<rapidjson::StringBuffer> &write, const std::string &host, std::string index) const {
    writeZabbixJsonItem<std::string>(write, host, "disk." + index + ".name", name, realTime);
    writeZabbixJsonItem<unsigned long long>(write, host, "disk." + index + ".capacity", capacity, realTime);
    writeZabbixJsonItem<unsigned long long>(write, host, "disk." + index + ".allocation", allocation, realTime);
    writeZabbixJsonItem<unsigned long long>(write, host, "disk." + index + ".physical", physical, realTime);
    writeZabbixJsonItem<long long>(write, host, "disk." + index + ".rd_req", rd_req, realTime);
    writeZabbixJsonItem<long long>(write, host, "disk." + index + ".rd_bytes", rd_bytes, realTime);
    writeZabbixJsonItem<long long>(write, host, "disk." + index + ".wr_req", wr_req, realTime);
    writeZabbixJsonItem<long long>(write, host, "disk." + index + ".wr_bytes", wr_bytes, realTime);
    writeZabbixJsonItem<long long>(write, host, "disk." + index + ".errs", errs, realTime);
    writeZabbixJsonItem<float>(write, host, "disk." + index + ".rd_speed", rd_speed, realTime);
    writeZabbixJsonItem<float>(write, host, "disk." + index + ".wr_speed", wr_speed, realTime);
}

void networkInfo::calculatorSpeed(const networkInfo &last) {
    int time_delta = realTime.tv_sec - last.realTime.tv_sec;
    rx_speed = (rx_bytes - last.rx_bytes) / (float)time_delta;
    tx_speed = (tx_bytes - last.tx_bytes) / (float)time_delta;
}

void networkInfo::write2Json(rapidjson::Writer<rapidjson::StringBuffer> &write) const {
    write.StartObject();
    write.Key("network_name");
    write.String(name.c_str());
    write.Key("rx_bytes");
    write.Int64(rx_bytes);
    write.Key("rx_packets");
    write.Int64(rx_packets);
    write.Key("rx_errs");
    write.Int64(rx_errs);
    write.Key("rx_drop");
    write.Int64(rx_drop);
    write.Key("tx_bytes");
    write.Int64(tx_bytes);
    write.Key("tx_packets");
    write.Int64(tx_packets);
    write.Key("tx_errs");
    write.Int64(tx_errs);
    write.Key("tx_drop");
    write.Int64(tx_drop);
    write.Key("rx_speed");
    write.Double(rx_speed);
    write.Key("tx_speed");
    write.Double(tx_speed);
    write.Key("clock");
    write.Int64(realTime.tv_sec);
    write.Key("ns");
    write.Int(realTime.tv_nsec);
    write.EndObject();
}

void networkInfo::write2ZabbixJson(rapidjson::Writer<rapidjson::StringBuffer> &write, const std::string &host, std::string index) const {
    writeZabbixJsonItem<std::string>(write, host, "net." + index + ".name", name, realTime);
    writeZabbixJsonItem<long long>(write, host, "net." + index + ".rx_bytes", rx_bytes, realTime);
    writeZabbixJsonItem<long long>(write, host, "net." + index + ".rx_packets", rx_packets, realTime);
    writeZabbixJsonItem<long long>(write, host, "net." + index + ".rx_errs", rx_errs, realTime);
    writeZabbixJsonItem<long long>(write, host, "net." + index + ".rx_drop", rx_drop, realTime);
    writeZabbixJsonItem<long long>(write, host, "net." + index + ".tx_bytes", tx_bytes, realTime);
    writeZabbixJsonItem<long long>(write, host, "net." + index + ".tx_packets", tx_packets, realTime);
    writeZabbixJsonItem<long long>(write, host, "net." + index + ".tx_errs", tx_errs, realTime);
    writeZabbixJsonItem<long long>(write, host, "net." + index + ".tx_drop", tx_drop, realTime);
    writeZabbixJsonItem<float>(write, host, "net." + index + ".rx_speed", rx_speed, realTime);
    writeZabbixJsonItem<float>(write, host, "net." + index + ".tx_speed", tx_speed, realTime);
}

void domMonitorData::calculatorUsageAndSpeed(const domMonitorData &last) {
    domInfo.calculatorUsage(last.domInfo);
    memStats.calculatorUsage(last.memStats);
    for (auto& diskStat : diskStats) {
        auto iter = last.diskStats.find(diskStat.first);
        if (iter != last.diskStats.end())
            diskStat.second.calculatorSpeed(iter->second);
    }
    for (auto& netStat : netStats) {
        auto iter = last.netStats.find(netStat.first);
        if (iter != last.netStats.end())
            netStat.second.calculatorSpeed(iter->second);
    }
}

std::string domMonitorData::toJsonString() const {
    rapidjson::StringBuffer strBuf;
    rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
    write.SetMaxDecimalPlaces(2);
    write.StartObject();
    write.Key("domain_name");
    write.String(domain_name.c_str());
    write.Key("delay");
    write.Uint(delay);
    domInfo.write2Json(write);
    memStats.write2Json(write);
    write.Key("disks");
    write.StartArray();
    for (const auto& diskStat : diskStats) {
        diskStat.second.write2Json(write);
    }
    write.EndArray();
    write.Key("networks");
    write.StartArray();
    for (const auto& netStat : netStats) {
        netStat.second.write2Json(write);
    }
    write.EndArray();
    write.Key("version");
    write.String(version.c_str());
    write.EndObject();
    return strBuf.GetString();
}

std::string domMonitorData::toZabbixString(const std::string &hostname) const {
    rapidjson::StringBuffer strBuf;
    rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
    write.SetMaxDecimalPlaces(2);
    write.StartObject();
    write.Key("request");
    write.String("agent data");
    write.Key("data");
    write.StartArray();
    domInfo.write2ZabbixJson(write, hostname);
    memStats.write2ZabbixJson(write, hostname);
    int index = 0;
    for (const auto& diskStat : diskStats) {
        diskStat.second.write2ZabbixJson(write, hostname, std::to_string(index++));
    }
    index = 0;
    for (const auto& netStat : netStats) {
        netStat.second.write2ZabbixJson(write, hostname, std::to_string(index++));
    }
    write.EndArray();
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    write.Key("clock");
    write.Int64(ts.tv_sec);
    write.Key("ns");
    write.Int(ts.tv_nsec);
    write.EndObject();
    return strBuf.GetString();
}

}
