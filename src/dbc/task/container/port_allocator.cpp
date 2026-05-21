// port_allocator.cpp — see port_allocator.h

#include "port_allocator.h"

#include "log/log.h"

#include <fstream>

namespace dbc::container {

int32_t PortAllocator::Configure(uint16_t start, uint16_t end) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (end <= start || (end - start) < 8) {
        LOG_ERROR << "invalid port range " << start << "-" << end;
        return E_DEFAULT;
    }
    range_start_ = start;
    range_end_ = end;
    free_ports_.clear();
    allocated_ports_.clear();
    for (uint16_t p = start; p < end; ++p) free_ports_.insert(p);
    return ERR_SUCCESS;
}

std::vector<uint16_t> PortAllocator::Allocate(uint8_t count) {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<uint16_t> result;
    if (free_ports_.size() < count) return result;
    auto it = free_ports_.begin();
    for (uint8_t i = 0; i < count && it != free_ports_.end(); ++i) {
        result.push_back(*it);
        allocated_ports_.insert(*it);
        it = free_ports_.erase(it);
    }
    if (!state_path_.empty()) SaveState(state_path_);
    return result;
}

void PortAllocator::Release(const std::vector<uint16_t>& ports) {
    std::lock_guard<std::mutex> lk(mtx_);
    for (uint16_t p : ports) {
        if (p < range_start_ || p >= range_end_) continue;
        allocated_ports_.erase(p);
        free_ports_.insert(p);
    }
    if (!state_path_.empty()) SaveState(state_path_);
}

int32_t PortAllocator::Reserve(const std::vector<uint16_t>& ports) {
    std::lock_guard<std::mutex> lk(mtx_);
    for (uint16_t p : ports) {
        if (p < range_start_ || p >= range_end_) return E_DEFAULT;
        free_ports_.erase(p);
        allocated_ports_.insert(p);
    }
    return ERR_SUCCESS;
}

int32_t PortAllocator::SaveState(const std::string& path) {
    // MVP-1: plain text "allocated:p1,p2,p3" — simple, atomic via rename.
    std::ofstream f(path + ".tmp");
    if (!f) return E_DEFAULT;
    f << range_start_ << " " << range_end_ << "\n";
    f << "allocated:";
    bool first = true;
    for (uint16_t p : allocated_ports_) {
        if (!first) f << ",";
        f << p;
        first = false;
    }
    f << "\n";
    f.close();
    std::rename((path + ".tmp").c_str(), path.c_str());
    state_path_ = path;
    return ERR_SUCCESS;
}

int32_t PortAllocator::LoadState(const std::string& path) {
    std::ifstream f(path);
    if (!f) return E_DEFAULT;
    std::lock_guard<std::mutex> lk(mtx_);
    uint16_t start, end;
    if (!(f >> start >> end)) return E_DEFAULT;
    Configure(start, end);   // resets sets
    std::string line;
    std::getline(f, line);   // eat \n after end
    std::getline(f, line);
    auto pos = line.find("allocated:");
    if (pos == std::string::npos) return ERR_SUCCESS;
    line = line.substr(pos + 10);
    std::vector<uint16_t> recovered;
    size_t i = 0;
    while (i < line.size()) {
        size_t j = line.find(',', i);
        std::string tok = line.substr(i, j == std::string::npos ? j : j - i);
        try { recovered.push_back(static_cast<uint16_t>(std::stoi(tok))); } catch (...) {}
        if (j == std::string::npos) break;
        i = j + 1;
    }
    Reserve(recovered);
    state_path_ = path;
    return ERR_SUCCESS;
}

size_t PortAllocator::FreeCount() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return free_ports_.size();
}

}  // namespace dbc::container
