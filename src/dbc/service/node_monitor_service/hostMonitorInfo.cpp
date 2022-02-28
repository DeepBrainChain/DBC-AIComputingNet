#include "hostMonitorInfo.h"
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

std::string hostMonitorData::toJsonString() const {
    rapidjson::StringBuffer strBuf;
    rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
    write.SetMaxDecimalPlaces(2);
    write.StartObject();
    write.Key("nodeId");
    write.String(nodeId.c_str());
    write.Key("delay");
    write.Uint(delay);
    write.Key("gpuCount");
    write.Uint(gpuCount);
    write.Key("gpuUsed");
    write.Uint(gpuUsed);
    write.Key("vmCount");
    write.Uint(vmCount);
    write.Key("vmRunning");
    write.Uint(vmRunning);
    write.Key("cpuUsage");
    write.Double(cpuUsage);
    write.Key("memTotal");
    write.Uint64(memTotal);
    write.Key("memFree");
    write.Uint64(memFree);
    write.Key("memUsage");
    write.Double(memUsage);
    write.Key("rxFlow");
    write.Int64(rxFlow);
    write.Key("txFlow");
    write.Int64(txFlow);
    write.Key("diskTotal");
    write.Uint64(diskTotal);
    write.Key("diskFree");
    write.Uint64(diskFree);
    write.Key("diskUsage");
    write.Double(diskUsage);
    write.Key("loadAverage");
    write.StartArray();
    for (const auto &num : loadAverage) {
        write.Double(num);
    }
    write.EndArray();
    write.Key("version");
    write.String(version.c_str());
    write.EndObject();
    return strBuf.GetString();
}

std::string hostMonitorData::toZabbixString() const {
    rapidjson::StringBuffer strBuf;
    rapidjson::Writer<rapidjson::StringBuffer> write(strBuf);
    write.SetMaxDecimalPlaces(2);
    write.StartObject();
    write.Key("request");
    write.String("agent data");
    write.Key("data");
    write.StartArray();
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    writeZabbixJsonItem<unsigned int>(write, nodeId, "host.gpuCount", gpuCount, ts);
    writeZabbixJsonItem<unsigned int>(write, nodeId, "host.gpuUsed", gpuUsed, ts);
    writeZabbixJsonItem<unsigned int>(write, nodeId, "host.vmCount", vmCount, ts);
    writeZabbixJsonItem<unsigned int>(write, nodeId, "host.vmRunning", vmRunning, ts);
    writeZabbixJsonItem<float>(write, nodeId, "host.cpuUsage", cpuUsage, ts);
    writeZabbixJsonItem<unsigned long long>(write, nodeId, "host.memTotal", memTotal, ts);
    writeZabbixJsonItem<unsigned long long>(write, nodeId, "host.memFree", memFree, ts);
    writeZabbixJsonItem<float>(write, nodeId, "host.memUsage", memUsage, ts);
    writeZabbixJsonItem<long long>(write, nodeId, "host.rxFlow", rxFlow, ts);
    writeZabbixJsonItem<long long>(write, nodeId, "host.txFlow", txFlow, ts);
    writeZabbixJsonItem<unsigned long long>(write, nodeId, "host.diskTotal", diskTotal, ts);
    writeZabbixJsonItem<unsigned long long>(write, nodeId, "host.diskFree", diskFree, ts);
    writeZabbixJsonItem<float>(write, nodeId, "host.diskUsage", diskUsage, ts);
    if (loadAverage.size() == 3) {
        writeZabbixJsonItem<float>(write, nodeId, "host.loadAverage.1", loadAverage[0], ts);
        writeZabbixJsonItem<float>(write, nodeId, "host.loadAverage.5", loadAverage[1], ts);
        writeZabbixJsonItem<float>(write, nodeId, "host.loadAverage.15", loadAverage[2], ts);
    }
    writeZabbixJsonItem<std::string>(write, nodeId, "host.dbcVersion", version, ts);
    write.EndArray();
    write.Key("clock");
    write.Int64(ts.tv_sec);
    write.Key("ns");
    write.Int(ts.tv_nsec);
    write.EndObject();
    return strBuf.GetString();
}

}
