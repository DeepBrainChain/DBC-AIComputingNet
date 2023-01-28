#include "resource_def.h"

int32_t DeviceCpu::total_cores() const {
    return sockets * cores_per_socket * threads_per_core;
}

std::string DeviceGpu::parse_bus(const std::string& id) {
    auto pos = id.find(':');
    if (pos != std::string::npos) {
        return id.substr(0, pos);
    } else {
        return "";
    }
}

std::string DeviceGpu::parse_slot(const std::string& id) {
    auto pos1 = id.find(':');
    auto pos2 = id.find('.');
    if (pos1 != std::string::npos && pos2 != std::string::npos) {
        return id.substr(pos1 + 1, pos2 - pos1 - 1);
    } else {
        return "";
    }
}

std::string DeviceGpu::parse_function(const std::string& id) {
    auto pos = id.find('.');
    if (pos != std::string::npos) {
        return id.substr(pos + 1);
    } else {
        return "";
    }
}
